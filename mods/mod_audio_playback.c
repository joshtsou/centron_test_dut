#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "mod_audio_playback.h"
#include "mod_ipc.h"
#include "socket.h"
#include "mod.h"

typedef struct connection {
    int ipc_sd;
    //int channel;
    ev_io rd_io;
	char mod_res[1024];
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
    
    return 0;
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
    struct audio_attr file_attr;
    int head_length = 0;
    int type_id = DECODE_PCM;
    int is_err = 0;

    switch(statemachine->stat) {
        case MOD_AUDIO_PLAYBACK_STATUS_START: {
            do {
                if(conn.ipc_sd)
                    break;
                if(mod_audio_playback_socket_connect(&conn)!=0) {
                    PPDEBUG(ctx, conn.mod_res, "socket connection failed");
                    is_err = 1;
                }
                sprintf(ctx->mod_name, "audio playback");
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
        case MOD_AUDIO_PLAYBACK_STATUS_SEND: {
            statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_FAILED;
            int ret = 0;
            int times = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_AUDIO_PLAYBACK_CMD_SET_PARAM_DATA_TIMES), "value")));
            do {
                FILE *fp = fopen(PLAY_BACK_AUDIO_PATH, "rb");
                if(!fp) {
                    PPDEBUG(ctx, conn.mod_res, "open audio file failed");
                    break;
                }
                if(audio_play_analyze_header(fp, &file_attr, &head_length, type_id) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "analyze audio file failed");
                    break;
                }
                PPDEBUG(ctx, conn.mod_res, "analyze audio file success. format: %d, channel: %d, rate: %d", file_attr.format, file_attr.channels, file_attr.rate);
                fclose(fp);
                statemachine->stat = MOD_AUDIO_PLAYBACK_STATUS_SUCCESS;
            }while(0);
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_SUCCESS: {
            statemachine->stat = state_success;
            break;
        }
        case MOD_AUDIO_PLAYBACK_STATUS_FAILED: {
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