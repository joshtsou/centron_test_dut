#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "jansson.h"
#include "ev.h"
#include "main.h"
#include "mod_sscmd.h"
#include "mod_ipc.h"

typedef struct connection {
    IPC_Socket *ipc;
    int channel;
    int stream;
    struct ev_loop *loop;
    ev_io rd_io;
    pthread_t conn_thread;
} conn_t;

static int mod_sscmd_is_init = 0;
static conn_t conn[MAX_CONNECTION_NUMBER] = {0};

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
#if 0
int h1n1_ss_ipc_read(struct connection *conn)
{
	struct h1n1_sscmd_header hdr;
	// int type = H1N1_SSCMD_REL_VID_FRAME_ACK;
	//char buf[1024];
	int n = IPC_FullRecv(conn->ss.ipc, (unsigned char *)&hdr, sizeof(hdr), 0);
	PDEBUGG("recv %d bytes", n);
	if(n <= 0 || n != sizeof(hdr))
	{
		// fixme: disconnec
		PDEBUG("Ch%d%d streamer is disconnected, read=%d, header size=%d", conn->channel, conn->stream, n, sizeof(hdr));
		h1n1_ss_drop(conn, "read fail", 0);
		return -1;
	}
	else
	{
		PDEBUGG("Ch%d%d, sscmd sync=%x, cmd=%d, length=%d", conn->channel, conn->stream, hdr.sync, hdr.cmd, hdr.length);
		ASSERT(hdr.cmd >= 0);
		ASSERT(hdr.cmd < H1N1_SSCMD_MAX);
		ASSERT(hdr.sync == H1N1_SSCMD_MAGIC_SYNC);
		struct h1n1_sscmd_media_idx idx;
		switch(hdr.cmd)
		{
		case H1N1_SSCMD_ACK:
			break;
		case H1N1_SSCMD_MEDIA_IDX:
			ASSERT(hdr.length == sizeof(idx));
			n = IPC_FullRecv(conn->ss.ipc, (unsigned char *)&idx, hdr.length, 0);
			PDEBUGG("recv %d bytes", n);
			PDEBUGG("Ch%d%d ss get %s media", conn->channel, conn->stream, type == H1N1_SSCMD_REL_VID_FRAME_ACK ? "Video" : "Audio");
			
			if (conn->ss.stat == H1N1_IPC_STAT_WAIT_ACK) {
				unsigned char* data_buffer = malloc(idx.length);
				PDEBUGG("Ch%d%d H1N1_IPC_STAT_WAIT_ACK and drop frame", conn->channel, conn->stream);
				assert(data_buffer);
				n = IPC_FullRecv(conn->ss.ipc, (unsigned char *)data_buffer, idx.length, 0);
				if(n != idx.length)
				{
					PDEBUG("Ch%d%d streamer is disconnected, read=%d, header size=%d", conn->channel, conn->stream, n, idx.length);
					free(data_buffer);
					return -1;
				}
				assert(idx.length > 0);
				free(data_buffer);
			}
			else {
				ASSERT(!conn->data_buffer);
				conn->data_buffer = malloc(idx.length);
				assert(conn->data_buffer);
				idx.offset = conn->data_buffer;
				n = IPC_FullRecv(conn->ss.ipc, (unsigned char *)idx.offset, idx.length, 0);
				PDEBUGG("recv %d bytes", n);
				if(n != idx.length)
				{
					PDEBUG("Ch%d%d streamer is disconnected, read=%d, header size=%d", conn->channel, conn->stream, n, idx.length);
					h1n1_ss_drop(conn, "read fail", 0);
					free(conn->data_buffer);
					conn->data_buffer = NULL;
					return -1;
				}
				// read succussfuly
				// PDEBUG("Ch%d%d ss get %s media idx=%d, len=%d, off=%u(%p), conn.stat=%d, ss.stat=%d", conn->channel, conn->stream, type == H1N1_SSCMD_REL_VID_FRAME_ACK ? "Video" : "Audio", idx.type, idx.length, idx.offset, idx.offset, conn->stat, conn->ss.stat);
				assert(idx.length > 0);
				conn->last_src_alive_time = time(NULL);
				conn->timeout_count = 0;

				if(h1n1_conn_sendto_media_data(conn, &idx, H1N1_MEDIA_STREAM_TYPE_LIVE) != 0)
				{
					PDEBUGG("sendto media failed due to network or frame error");
				}
				if(conn->streaming_mode == H1N1_VIDEO_EVENTKEY_CAM_VIDEO)
				{
					if(rsdvd_backup_is_ready_to_start(conn->rsdvd) && conn->rsdvd_length <= H1N1_RSDVD_BUFFER_SIZE 
						&& rsdvd_backup_push_frame(conn->rsdvd, H1N1_RSDVD_TYPE_MEDIA_FRAME, &idx, sizeof(idx), my_media_data_free, conn) == 0)
					{
						conn->rsdvd_length += idx.length;
						if(conn->rsdvd_length > H1N1_RSDVD_BUFFER_SIZE)
						{
							struct rsdvd_header hdr;
							hdr.event_time = conn->last_src_alive_time;
							hdr.movie_time = 0;
							rsdvd_backup_push_frame(conn->rsdvd, H1N1_RSDVD_TYPE_END, &hdr, sizeof(hdr), NULL, conn);
							rsdvd_backup_end(conn->rsdvd);
							rsdvd_backup_pop_last_ready(conn->rsdvd);
						}
					}
					else
					{
						PDEBUGG("rsdvd frame loss");
						free(conn->data_buffer);
					}
				}
				else
				{
					free(conn->data_buffer);
				}

				conn->data_buffer = NULL;
			}
			// HonoStreamerReleaseFrame(obj, type);
			// h1n1_ss_release_frame(conn, type);
			//HonoStreamerReleaseFrame(obj, type);
			PDEBUGG("idx process is over");
			break;
		case H1N1_SSCMD_SDP_ACK:
			assert(sizeof(conn->sdp_cache) > hdr.length);
			n = IPC_FullRecv(conn->ss.ipc, (unsigned char *)conn->sdp_cache, hdr.length, 0);
			if(n != hdr.length)
			{
				PDEBUG("except %d bytes, real got= %d bytes", hdr.length, n);
				PDEBUG("%s", conn->sdp_cache);
				conn->sdp_cache[0] = 0;
				break;
			}
			else if(conn->sdp_cache[0] == 0)
			{
				//not ready
				break;
			}
			assert(n == hdr.length);
			PDEBUG("Ch%d%d gets SDP:%s", conn->channel, conn->stream, conn->sdp_cache);
			if(strstr(conn->sdp_cache, "AMR/8000"))
				conn->audio_type = H1N1_MEDIA_AUDIO_TYPE_AMRNB;
			if(strstr(conn->sdp_cache, "mode=AAC-hbr"))
				conn->audio_type = H1N1_MEDIA_AUDIO_TYPE_AAC;
			break;
		case H1N1_SSCMD_MEDIA_ACK_FAIL:
		case H1N1_SSCMD_MEDIA_ACK_TIMEOUT:
			break;
		case H1N1_SSCMD_MEDIA_ACK_EX_STOP:
			PDEBUG("Ch%d%d recv H1N1_SSCMD_MEDIA_ACK_EX_STOP (stat=%d)", conn->channel, conn->stream, conn->ss.stat);
			if (conn->ss.stat == H1N1_IPC_STAT_WAIT_ACK) {
				conn->ss.transfer_stat = H1N1_IPC_STAT_READY;
				//PDEBUG("Ch%d%d h1n1_ss_request_ex_play (%s)", conn->channel, conn->stream, __func__);
				//h1n1_ss_request_ex_play(conn, conn->event_time, conn->movie_time);
			}
			break;
		default:
			PDEBUG("Ch%d%d, unknown sscmd cmd=%d, sync=%x, length=%d", conn->channel, conn->stream, hdr.cmd, hdr.sync, hdr.length);
			assert(0);
		}
	}
	return 0;
}
#endif
static void sscmd_recv_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
    conn_t *con = (conn_t*)w->data;
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
    return 0;
}

int mod_sscmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_err = 0;
    switch(statemachine->stat) {
        case MOD_SSCMD_STATUS_START:
            do {
                for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                    for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                        int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
                        if(!ev_is_active(&conn[idx].rd_io)) {
                            pthread_create(&conn[idx].conn_thread, NULL, mod_sscmd_recv_thread, &conn[idx]);
                            pthread_detach(conn[idx].conn_thread);
                        }
                    }
                }
                if(mod_sscmd_is_init)
                    break;
                //connect ss
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
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            }
            else {
                mod_sscmd_is_init = 1;
                statemachine->stat = MOD_SSCMD_STATUS_START + ctx->ipc_header.cmd;
            }
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP:
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
                statemachine->stat = MOD_SSCMD_STATUS_FAILED;
            }else {
                statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            }
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_SUCCESS:
            TDEBUG("MOD_SSCMD_STATUS_SUCCESS");
            statemachine->stat = state_success;
            break;
        case MOD_SSCMD_STATUS_FAILED:
            TDEBUG("MOD_SSCMD_STATUS_FAILED");
            statemachine->stat = state_failed;
            break;
        default:
            return 0;
            break;
    }
    return 1;
}

