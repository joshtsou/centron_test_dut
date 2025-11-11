#include <stdio.h>
#include <stdlib.h>
#include<unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include "jansson.h"
#include "ev.h"
#include "main.h"
#include "mod_sscmd.h"
#include "mod_ipc.h"
#include "socket.h"

typedef struct connection {
    IPC_Socket *ipc;
    int channel;
    int stream;
    struct ev_loop *loop;
    ev_io rd_io;
	char sdp_cache[1024];
	char test_report[1024];
    pthread_t conn_thread;
} conn_t;

static int mod_sscmd_is_init = 0;
static conn_t conn[MAX_CONNECTION_NUMBER] = {0};

void mod_ssmcd_test_result_append(char *input, char *target, int input_size, int target_size) {
	char header[64] = "[result sscmd] ";
	char tail[64] = "\n";
	do {
		if(!input || !target) {
			PDEBUG("test result append error.");
			break;
		}
		int remain_size = target_size - strlen(target) - strlen(header) - strlen(tail) - 1;
		if(remain_size < input_size) {
			PDEBUG("test result append error: no space.");
			break;
		}
		strncat(target, header, remain_size);
		strncat(target, input, remain_size);
		strncat(target, tail, remain_size);
	}while(0);
}

int h1n1_ss_request_command(conn_t *conn, int command, unsigned char *data, int datalen)
{
	struct h1n1_sscmd_header hdr;
	if(!IPC_SOCKET_CHECK(conn->ipc))
	{
		//fixme: how to handle this exception
		PDEBUG("Ch%d%d send cmd=%d fail due to ipc channel failure", conn->channel, conn->stream, command);
		return -1;
	}
	hdr.sync = H1N1_SSCMD_MAGIC_SYNC;
	hdr.cmd = command;
	hdr.length = datalen;
	//PDEBUGG("Ch%d,%d send command %d", conn->channel, conn->stream, command);
	int n = IPC_Send(conn->ipc, (unsigned char *)&hdr, sizeof(hdr));
	if(n < 0)
	{
		//fixme: how to handle this exception
		PDEBUG("send cmd=%d fail due to ipc channel failure", command);
		return -1;
	}
	if(data && datalen > 0 && IPC_Send(conn->ipc, data, datalen) <= 0)
	{
		//fixme: how to handle this exception
		PDEBUG("send cmd=%d fail due to ipc channel failure", command);
		return -1;
	}
	return 0;
}
#if 1
int h1n1_ss_ipc_read(conn_t *conn)
{
	struct h1n1_sscmd_header hdr;
	// int type = H1N1_SSCMD_REL_VID_FRAME_ACK;
	//char buf[1024];
	int n = IPC_FullRecv(conn->ipc, (unsigned char *)&hdr, sizeof(hdr), 0);
	char test_result[1024] = "\0";
	if(n <= 0 || n != sizeof(hdr))
	{
		// fixme: disconnec
		TDEBUG("RECV SSCMD header failed: len = %d, size not match. channel: %d, stream: %d, conn_fd: %d",n, conn->channel, conn->stream, conn->ipc->fd);
		return -1;
	}
	else
	{
		if(hdr.cmd < 0 || hdr.cmd >= H1N1_SSCMD_MAX) {
			TDEBUG("SSCMD Header error: cmd out of range");
			return -1;
		}
		if(hdr.sync != H1N1_SSCMD_MAGIC_SYNC) {
			TDEBUG("SSCMD Header error: masgic number not correct");
			return -1;
		}
		struct h1n1_sscmd_media_idx idx;
		switch(hdr.cmd)
		{
		case H1N1_SSCMD_ACK:
			TDEBUG("RECV H1N1_SSCMD_ACK, channel: %d, stream: %d", conn->channel, conn->stream);
			break;
		case H1N1_SSCMD_MEDIA_IDX:
			TDEBUG("RECV H1N1_SSCMD_MEDIA_IDX, channel: %d, stream: %d", conn->channel, conn->stream);
			//ASSERT(hdr.length == sizeof(idx));
			n = IPC_FullRecv(conn->ipc, (unsigned char *)&idx, hdr.length, 0);
			if(n == sizeof(struct h1n1_sscmd_media_idx)){
				unsigned char* data_buffer = malloc(idx.length);
				n = IPC_FullRecv(conn->ipc, (unsigned char *)data_buffer, idx.length, 0);
				if(n != idx.length)
				{
					TDEBUG("H1N1_SSCMD_MEDIA_IDX recv error. data length is not match idx.length");
					free(data_buffer);
					return -1;
				}
				else {
					sprintf(test_result, "H1N1_SSCMD_MEDIA_IDX recv ok, channel:%d, stream: %d", conn->channel, conn->stream);
					TDEBUG("%s" ,test_result);
				}
				free(data_buffer);
			}
			else {
				TDEBUG("H1N1_SSCMD_MEDIA_IDX recv error. recv length not match idx structure size");
			}
			break;
		case H1N1_SSCMD_SDP_ACK:
			TDEBUG("RECV H1N1_SSCMD_SDP_ACK, channel: %d, stream: %d", conn->channel, conn->stream);
			//assert(sizeof(conn->sdp_cache) > hdr.length);
			n = IPC_FullRecv(conn->ipc, (unsigned char *)conn->sdp_cache, hdr.length, 0);
			if(n != hdr.length)
			{
				conn->sdp_cache[0] = 0;
				break;
			}
			//assert(n == hdr.length);
			TDEBUG("SDP:%s", conn->sdp_cache);
			break;
		case H1N1_SSCMD_MEDIA_ACK_FAIL:
		case H1N1_SSCMD_MEDIA_ACK_TIMEOUT:
			break;
		case H1N1_SSCMD_MEDIA_ACK_EX_STOP:
			TDEBUG("RECV H1N1_SSCMD_MEDIA_ACK_EX_STOP, channel: %d, stream: %d", conn->channel, conn->stream);
			break;
		default:
			TDEBUG("RECV unknown sscmd cmd=%d, sync=%x, length=%d, channel: %d, stream: %d", hdr.cmd, hdr.sync, hdr.length, conn->channel, conn->stream);
			//assert(0);
		}
	}
	return 0;
}
#endif
static void sscmd_recv_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
	conn_t *con = (conn_t*)w->data;
	int ret = 0;
	// int ret = 0;
	// fd_set rfds;
	// FD_ZERO(&rfds);
	// FD_SET(IPC_Select_Object(con->ipc), &rfds);
	// ret = select(IPC_Select_Object(con->ipc) + 1, &rfds, NULL, NULL, NULL);
	// if(ret>0 && FD_ISSET(IPC_Select_Object(con->ipc), &rfds)) {
		TDEBUG("SSCMD RECVing ... channel: %d, stream: %d, conn_fd: %d, active_fd: %d", con->channel, con->stream, con->ipc->fd, w->fd);
		ret = h1n1_ss_ipc_read(con);
	// }
    //ev_io_stop(loop, w);
	if(ret == -1)
		ev_io_stop(loop, w);
}

void* mod_sscmd_recv_thread(void *arg) {
    conn_t *recv_conn = (conn_t*)arg;
    if(!IPC_SOCKET_CHECK(recv_conn->ipc))
	{
		PDEBUG("Ch%d stream%d fail due to ipc recv", recv_conn->channel, recv_conn->stream);
		return 0;
	}
    recv_conn->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);
    ev_io_init(&recv_conn->rd_io, sscmd_recv_io_callback, IPC_Select_Object(recv_conn->ipc), EV_READ);
    ev_io_start(recv_conn->loop, &recv_conn->rd_io);
    recv_conn->rd_io.data = recv_conn;
    ev_run(recv_conn->loop, 0);
	PDEBUG("recv thread finished. channel: %d, stream: %d", recv_conn->channel, recv_conn->stream);
    return 0;
}

int mod_sscmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_err = 0;
	char res_message[128] = "\0";
    switch(statemachine->stat) {
        case MOD_SSCMD_STATUS_START:
            do {
                if(mod_sscmd_is_init)
                    break;
                for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                    for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                        int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
                        conn[idx].channel = ch;
                        conn[idx].stream = s;
                        conn[idx].ipc = IPC_Create_Client(MEDIA_SOCKET);
                        if(!conn[idx].ipc) {
                            PDEBUG("MOD SSCMD CONNECT IPC ERROR, channel: %d, stream: %d", ch, s);
                            is_err |= 1;
                        }
                        else {
                            TDEBUG("MOD SSCMD IPC CONNECT SUCCESS, channel: %d, stream: %d", ch, s);
                        }
                    }
                }
            }while(0);
            if(is_err) {
                //statemachine->stat = MOD_SSCMD_STATUS_START + ctx->ipc_header.cmd;
				sprintf(res_message, "MOD_SSCMD CONNECT IPC ERROR");
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            }
            else {
                mod_sscmd_is_init = 1;
                //statemachine->stat = MOD_SSCMD_STATUS_START + ctx->ipc_header.cmd;
				statemachine->stat = MOD_SSCMD_STATUS_RECV_THREAD;
            }
            break;
		case MOD_SSCMD_STATUS_RECV_THREAD:
			TDEBUG("MOD_SSCMD_STATUS_RECV_THREAD");
			for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
				for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
					int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
					if(!ev_is_active(&conn[idx].rd_io)) {
						PDEBUG("recv thread created, ch:%d, stream:%d", ch, s);
						pthread_create(&conn[idx].conn_thread, NULL, mod_sscmd_recv_thread, &conn[idx]);
						pthread_detach(conn[idx].conn_thread);
					}
				}
			}
			statemachine->stat = MOD_SSCMD_STATUS_RECV_THREAD + ctx->ipc_header.cmd;
			break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP:
			usleep(500000);
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP");
            is_err = 0;
            for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                    int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
                    struct h1n1_sscmd_media_sdp req_sdp_arg = {0};
                    req_sdp_arg.channel = conn[idx].channel;
                    req_sdp_arg.stream = conn[idx].stream;
                    if(h1n1_ss_request_command(&conn[idx], H1N1_SSCMD_MEDIA_SDP, (unsigned char *)&req_sdp_arg, sizeof(req_sdp_arg))!=0) {
                        is_err |= 1;
                        TDEBUG("MOD SSCMD H1N1_SSCMD_MEDIA_SDP ERROR, channel: %d, stream: %d",ch, s);
                    }
                }
            }
            if(is_err) {
				sprintf(res_message, "MOD_SSCMD H1N1_SSCMD_MEDIA_SDP ERROR");
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            } else {
                statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            }
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY: {
			usleep(500000);
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY");
			is_err = 0;
			//int sd_mode = atoi(json_string_value(json_object_get(json_array_get(ctx->data, CMD_EX_PLAY_DATA_SD_STATUS), "value")));
			int nc_status = atoi(json_string_value(json_object_get(json_array_get(ctx->data, CMD_EX_PLAY_DATA_NC_STATUS), "value")));
			int rec_mode = atoi(json_string_value(json_object_get(json_array_get(ctx->data, CMD_EX_PLAY_DATA_REC_MODE), "value")));
			//TDEBUG("sd_mode: %d, nc_status: %d, rec_mode: %d", sd_mode, nc_status, rec_mode);
			struct h1n1_sscmd_media_stream arg;
			for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                    int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
					sprintf(arg.camid, "%d", idx);
					arg.connected = nc_status;
					arg.type = rec_mode == 0 ? H1N1_VIDEO_EVENTKEY_CAM_VIDEO : H1N1_VIDEO_EVENTKEY_CAM_SAVE1;
					arg.event_time = time(NULL);
					arg.movie_time = time(NULL) - POST_BUFFER;
					arg.batch_length = BATCH_LENGTH;
					arg.local_rec = 0;
					if(h1n1_ss_request_command(&conn[idx], H1N1_SSCMD_MEDIA_IDX, (unsigned char*)&arg, sizeof(struct h1n1_sscmd_media_stream)) != 0) {
                        is_err |= 1;
                        TDEBUG("MOD SSCMD H1N1_SSCMD_MEDIA_IDX ERROR, channel: %d, stream: %d",ch, s);
                    }
				}
			}
			if(is_err) {
				sprintf(res_message, "MOD SSCMD H1N1_SSCMD_MEDIA_IDX ERROR");
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            } else {
                statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            }
            //statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
		}
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP:
			usleep(500000);
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP");
			is_err = 0;
			for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                    int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
					if(h1n1_ss_request_command(&conn[idx], H1N1_SSCMD_MEDIA_EX_STOP, NULL, 0) != 0) {
						is_err |= 1;
                        TDEBUG("MOD SSCMD H1N1_SSCMD_MEDIA_EX_STOP ERROR, channel: %d, stream: %d",ch, s);
					}
				}
			}
			if(is_err) {
				sprintf(res_message, "MOD SSCMD H1N1_SSCMD_MEDIA_EX_STOP ERROR");
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            } else {
                statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            }
            //statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE: {
			usleep(500000);
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE");
			is_err = 0;
			int nc_status = atoi(json_string_value(json_object_get(json_array_get(ctx->data, CMD_EX_PLAY_DATA_NC_STATUS), "value")));
			int rec_mode = atoi(json_string_value(json_object_get(json_array_get(ctx->data, CMD_EX_PLAY_DATA_REC_MODE), "value")));
			struct h1n1_sscmd_media_stream arg;
			for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                    int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
					sprintf(arg.camid, "%d", idx);
					arg.type = rec_mode == 0 ? H1N1_VIDEO_EVENTKEY_CAM_VIDEO : H1N1_VIDEO_EVENTKEY_CAM_SAVE1;
					arg.connected = nc_status;
					arg.batch_length = BATCH_LENGTH;
					arg.local_rec = 0;
					if(h1n1_ss_request_command(&conn[idx], H1N1_SSCMD_MEDIA_NC_STATE, (unsigned char*)&arg, sizeof(struct h1n1_sscmd_media_stream)) != 0) {
						is_err |= 1;
                        TDEBUG("MOD SSCMD H1N1_SSCMD_MEDIA_NC_STATE ERROR, channel: %d, stream: %d",ch, s);
					}
				}
			}
			if(is_err) {
				sprintf(res_message, "MOD SSCMD H1N1_SSCMD_MEDIA_NC_STATE ERROR");
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            } else {
                statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            }
            //statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
		}
        case MOD_SSCMD_STATUS_SUCCESS:
            TDEBUG("MOD_SSCMD_STATUS_SUCCESS");
			char success_message[128] = "\0";
			sprintf(success_message, "MOD SSCMD SEND SUCCESS, CMD: %d", ctx->ipc_header.cmd);
			ipc_header_t header;
			header.mod = ctx->ipc_header.mod;
			header.cmd = ctx->ipc_header.cmd;
			header.len = strlen(success_message);
			socket_mcast_reply(ctx->multi_sockfd, REPLY_PORT, &header, sizeof(ipc_header_t), &ctx->remote_addr);
			socket_mcast_reply(ctx->multi_sockfd, REPLY_PORT, success_message, strlen(success_message), &ctx->remote_addr);
            statemachine->stat = state_success;
            break;
        case MOD_SSCMD_STATUS_FAILED:
            TDEBUG("MOD_SSCMD_STATUS_FAILED");
			socket_mcast_reply(ctx->multi_sockfd, REPLY_PORT, res_message, strlen(res_message), &ctx->remote_addr);
            statemachine->stat = state_failed;
            break;
        default:
            return 0;
            break;
    }
    return 1;
}

