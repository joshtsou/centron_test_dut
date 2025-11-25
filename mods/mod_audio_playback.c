#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "mod_audio_playback.h"
#include "mod_ipc.h"
#include "socket.h"
#include "mod.h"

typedef struct connection {
    int ipc_sd;
    int times;
    ev_io rd_io;
	char mod_res[1024];
    int head_length;
    struct audio_attr file_attr;
    //char *mod_recv_data;
    //int recv_len;
} conn_t;

typedef enum {
	DECODE_PCM = 0,
	DECODE_AAC
}DECODE_TYPE;

#define RTS_AUDIO_HEADER_LENGTH_MAX	0x44

static conn_t conn = {0};
//int static mod_snapshot_is_init = 0;

int audio_play_analyze_header(FILE *fp, struct audio_attr *attr, int *head_length, int type_id)
{
	int ret;
	int cksize;
	char buffer[RTS_AUDIO_HEADER_LENGTH_MAX];

	if (type_id == DECODE_PCM) {
		ret = fread(buffer, 1, RTS_AUDIO_HEADER_LENGTH_MAX, fp);
		if (ret != RTS_AUDIO_HEADER_LENGTH_MAX) {
			ret = -1;
            PDEBUG("read audio file failed");
			goto exit;
		}

		attr->channels = (unsigned int)(buffer[0x16] & 0xff) |
				((unsigned int)(buffer[0x17] & 0xff) << 8);
		attr->rate = (unsigned int)(buffer[0x18] & 0xff) |
				((unsigned int)(buffer[0x19] & 0xff) << 8) |
				((unsigned int)(buffer[0x1a] & 0xff) << 16) |
				((unsigned int)(buffer[0x1b] & 0xff) << 24);
		attr->format = (unsigned int)(buffer[0x22] & 0xff) |
				((unsigned int)(buffer[0x23] & 0xff) << 8);
		cksize = (unsigned int)(buffer[0x10] & 0xff) |
			((unsigned int)(buffer[0x11] & 0xff) << 8) |
			((unsigned int)(buffer[0x12] & 0xff) << 16) |
			((unsigned int)(buffer[0x13] & 0xff) << 24);
		switch (cksize) {
		case 16:
			*head_length = 0x2C;
			break;
		case 18:
			*head_length = 0x2E;
			break;
		case 40:
			*head_length = 0x44;
			break;
		default:
            PDEBUG("audio header size analyze failed");
			ret = -2;
			goto exit;
		}
		ret = 0;
	}
	else {
        PDEBUG("typedID incorrect. error");
		ret = -1;
	}
exit:
	return ret;
}

static int fullwrite(int to, char *data, int len, int timeout)
{
	int total = 0;
	time_t t1 = time(NULL), t2;
	while (total != len)
	{
		int rlen = write(to, data + total, len - total);
		if (rlen <= 0 && errno != EINTR && errno != EAGAIN)
			return -1;
		total += rlen;
		t2 = time(NULL);
		if (t2 - t1 > timeout)
			return -1;
		if (t1 > t2)
			t1 = t2; // time offset
	}
	return total;
}

int audio_play_get_frame_size(struct audio_attr *attr)
{
	return attr->rate / 125 * 4 * attr->channels * attr->format / 8;
}

int handle_playback_pcm(int cs, const char* path, struct audio_attr *pfile_attr, int head_length)
{
    int read, sent, size = audio_play_get_frame_size(pfile_attr);
    char *buffer = (char*)malloc(size);
    int all = 0;
    int ret = -1;

    FILE *fp = fopen(path, "rb");

    if(!fp)
        goto end;
    if(head_length)
        fseek(fp, head_length, SEEK_SET);
    do {
        read = fread(buffer, 1, size, fp);
        if(read) {
            if((sent = fullwrite(cs, buffer, read, 10)) == -1) {
                goto end;
            }
            all += sent;
            //TDEBUG("audio callback sending... send total: %d bytes", all);
        }
    }while(read);
    ret = 0;
end:
    if(buffer)
        free(buffer);
    return ret;
}

int mod_audio_playback_socket_connect(conn_t *conn) {
    int ret = -1;
    struct sockaddr_un un;

    if(conn->ipc_sd != 0) {
        close(conn->ipc_sd);
        conn->ipc_sd = 0;
    }
    do {
        conn->ipc_sd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (conn->ipc_sd == -1) {
            PDEBUG("create socket fail");
            break;
        }

        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, PLAYBACK_SOCKET);

        if (connect(conn->ipc_sd, (struct sockaddr*)&un, sizeof(un)) != 0) {
            close(conn->ipc_sd);
            conn->ipc_sd = 0;
            PDEBUG("socket connection error");
            break;
        }
        ret = 0;
    }while(0);
    return ret;
}

int ipc_audio_playback_request_recv(conn_t *conn, main_ctx* ctx) {
    ipcmsg_hdr_t hdr;
    ipcmsg_resp_code_t rc;
    statemachine_t *pstate = ctx->statemachine;
    int ret = -1;
    int r = recv(conn->ipc_sd, &hdr, sizeof(hdr), MSG_WAITALL);
    pstate->stat = MOD_AUDIO_PLAYBACK_STATUS_FAILED;
    do {
        if(r != sizeof(hdr) || hdr.type != IPCMSG_TYPE_RESPONSE_CODE) {
            PPDEBUG(ctx, conn->mod_res, "recv header error.");
            break;
        }
        r = recv(conn->ipc_sd, &rc, sizeof(rc), MSG_WAITALL);
        if(r != sizeof(rc) || rc.code != IPCMSG_RESP_CODE_OK) {
            PPDEBUG(ctx, conn->mod_res, "recv resp code error");
            break;
        }
        
        pstate->stat = MOD_AUDIO_PLAYBACK_STATUS_SEND;
        ret = 0;
    }while(0);
    return ret;
}

static void audio_playback_reciever_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
	conn_t *con = (conn_t*)w->data;
	main_ctx *ctx = (main_ctx*)ev_userdata(loop);
	//int ret = 0;
	//PPDEBUG(ctx, con->mod_res, "PTZCMD RECVing ... active_fd: %d", w->fd);
    if(ipc_audio_playback_request_recv(con, ctx) != 0) {
        PPDEBUG(ctx, con->mod_res, "recv error.");
    }
    else {
        PPDEBUG(ctx, con->mod_res, "recv success");
    }
    close(w->fd);
    con->ipc_sd = 0;
    ev_io_stop(loop, w);
}

int mod_audio_playback_reciever_create(conn_t *conn, main_ctx *ctx) {
    if(!conn->ipc_sd)
	{
		PDEBUG("%s fail due to ev_io init", ctx->mod_name);
		return -1;
	}
	conn->rd_io.data = conn;
    ev_io_init(&conn->rd_io, audio_playback_reciever_io_callback, conn->ipc_sd, EV_READ);
    ev_io_start(ctx->loop, &conn->rd_io);
    return 0;
}

int mod_audio_playback_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int type_id = DECODE_PCM;
    int is_err = 0;

    switch(statemachine->stat) {
        case MOD_AUDIO_PLAYBACK_STATUS_START: {
            do {
                sprintf(ctx->mod_name, "audio playback");
                if(conn.ipc_sd)
                    break;
                if(mod_audio_playback_socket_connect(&conn)!=0) {
                    PPDEBUG(ctx, conn.mod_res, "socket connection failed");
                    is_err = 1;
                }
            }while(0);
            if(is_err) {
                statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_FAILED;
            }
            else {
                if(!ev_is_active(&conn.rd_io)) {
                    PDEBUG("recv thread created");
                    mod_audio_playback_reciever_create(&conn, ctx);
                }
                statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_START + ctx->ipc_header.cmd;
            }
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_CHECK: {
            statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_FAILED;
            conn.times = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_AUDIO_PLAYBACK_CMD_SET_PARAM_DATA_TIMES), "value")));
            char *req_pcm = "application/audio-pcm";
            do {
                if(conn.times < 1 || conn.times > 5) {
                    PPDEBUG(ctx, conn.mod_res,"you got to play too few or too many times, failed. times: %d", conn.times);
                    break;
                }
                FILE *fp = fopen(PLAY_BACK_AUDIO_PATH, "rb");
                if(!fp) {
                    PPDEBUG(ctx, conn.mod_res, "open audio file failed");
                    break;
                }
                if(audio_play_analyze_header(fp, &conn.file_attr, &conn.head_length, type_id) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "analyze audio file failed");
                    break;
                }
                PPDEBUG(ctx, conn.mod_res, "analyze audio file success. format: %d, channel: %d, rate: %d", conn.file_attr.format, conn.file_attr.channels, conn.file_attr.rate);
                fclose(fp);
                ipcmsg_hdr_t hdr = {.type = IPCMSG_TYPE_REQUEST_AUDIO_OUT, .length = strlen(req_pcm) + 1};
                write(conn.ipc_sd, &hdr, sizeof(hdr));
                write(conn.ipc_sd, req_pcm, hdr.length);
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_SEND: {
            PPDEBUG(ctx, conn.mod_res, "sending audio playback...");
            int ret = -1;
            do {
                if(handle_playback_pcm(conn.ipc_sd, PLAY_BACK_AUDIO_PATH, &conn.file_attr, conn.head_length) == -1) {
                    PPDEBUG(ctx, conn.mod_res, "send audio playback error");
                    break;
                }else {
                    PPDEBUG(ctx, conn.mod_res, "send audio playback success, time remain: %d", --conn.times);
                }
                ret = 0;
            }while(0);
            if(ret != 0) {
                statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_FAILED;
            }else {
                if(conn.times) {
                    statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_SEND;
                }
                else {
                    statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_SUCCESS;
                }
            } 
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_SUCCESS: {
            PPDEBUG(ctx, conn.mod_res, "test finished, success");
            close(conn.ipc_sd);
            conn.ipc_sd = 0;
            statemachine->stat = state_success;
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_FAILED: {
            PPDEBUG(ctx, conn.mod_res, "test finished, failed");
            close(conn.ipc_sd);
            conn.ipc_sd = 0;
            statemachine->stat = state_failed;
            break;
        }
        default: {
            return 0;
            break;
        }
    }
    return 1;
}