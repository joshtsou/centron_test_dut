#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "mod_snapshot.h"
#include "mod_ipc.h"
#include "socket.h"
#include "mod.h"

typedef struct connection {
    int ipc_sd;
    int channel;
    ev_io rd_io;
	char mod_res[1024];
    char *mod_recv_data;
    int recv_len;
} conn_t;

static conn_t conn = {0};
int static mod_snapshot_is_init = 0;

int mod_snapshot_socket_connect(conn_t *conn) {
    int ret = -1;
    struct timeval timeout;
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

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(conn->ipc_sd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
        setsockopt(conn->ipc_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, SNAPSHOT_SOCKET);

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

int ipc_snapshot_request_recv(conn_t *conn, main_ctx* ctx) {
    if(!conn->mod_recv_data) {
        return -1;
    }
    int off = 0;
    int ret = 0;
    do {
        ret = recv(conn->ipc_sd, conn->mod_recv_data + off, conn->recv_len - off, MSG_NOSIGNAL);
        if(ret == -1) {
            PDEBUG("snapshot recv -1 failed");
            return -1;
        }
        else if(ret == 0) {
            PPDEBUG(ctx, conn->mod_res, "recv data finished, ret 0");
            break;
        }
        else {
            PPDEBUG(ctx, conn->mod_res, "recv data, length: %d", ret);
            off+=ret;
        }
    } while(1);
    return 0;
}

static void snapshot_reciever_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
	conn_t *con = (conn_t*)w->data;
	main_ctx *ctx = (main_ctx*)ev_userdata(loop);
	//int ret = 0;
	//PPDEBUG(ctx, con->mod_res, "PTZCMD RECVing ... active_fd: %d", w->fd);
    if(ipc_snapshot_request_recv(con, ctx) != 0) {
        PPDEBUG(ctx, con->mod_res, "recv error.");
    }
    else {
        PPDEBUG(ctx, con->mod_res, "recv success");
    }
    if(con->mod_recv_data)
        free(con->mod_recv_data);
    close(w->fd);
    con->ipc_sd = 0;
    mod_snapshot_is_init = 0;
    ev_io_stop(loop, w);
}

int mod_snapshot_reciever_create(conn_t *conn, main_ctx *ctx) {
    if(!conn->ipc_sd)
	{
		PDEBUG("%s fail due to ev_io init", ctx->mod_name);
		return -1;
	}
	conn->rd_io.data = conn;
    ev_io_init(&conn->rd_io, snapshot_reciever_io_callback, conn->ipc_sd, EV_READ);
    ev_io_start(ctx->loop, &conn->rd_io);
    return 0;
}

int mod_snapshot_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_err = 0;
    switch(statemachine->stat) {
        case MOD_SNAPSHOT_STATUS_START: {
            do {
                if(mod_snapshot_is_init)
                    break;
                if(mod_snapshot_socket_connect(&conn)!=0) {
                    PPDEBUG(ctx, conn.mod_res, "socket connection failed");
                    is_err = 1;
                }
                sprintf(ctx->mod_name, "snapshot");
            }while(0);
            if(!conn.mod_recv_data) {
                free(conn.mod_recv_data);
                conn.mod_recv_data = NULL;
            }
            if(is_err) {
                statemachine->stat = MOD_SNAPSHOT_STATUS_FAILED;
            }
            else {
                if(!ev_is_active(&conn.rd_io)) {
                    PDEBUG("recv thread created");
                    mod_snapshot_reciever_create(&conn, ctx);
                }
                conn.mod_res[0] = '\0';
                mod_snapshot_is_init = 1;
                statemachine->stat = MOD_SNAPSHOT_STATUS_START + ctx->ipc_header.cmd;
            }
            break;
        }
        case MOD_SNAPSHOT_STATUS_GO: {
            statemachine->stat = MOD_SNAPSHOT_STATUS_FAILED;
            int ret = 0;
            int channel = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_SNAPSHOT_CMD_SET_PARAM_DATA_CHANNEL), "value")));
            int width = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_SNAPSHOT_CMD_SET_PARAM_DATA_WIDTH), "value")));
            int height = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_SNAPSHOT_CMD_SET_PARAM_DATA_HEIGHT), "value")));
            int quality = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_SNAPSHOT_CMD_SET_PARAM_DATA_QUALITY), "value")));
            do {
                char cmd[128] = "\0";
                conn.recv_len = width * height * 2;
                conn.mod_recv_data = (char*)malloc(conn.recv_len);
                sprintf(cmd, "%d@%dx%d@%d", channel, width, height, quality);
                ret = send(conn.ipc_sd, cmd, strlen(cmd) + 1, MSG_NOSIGNAL);
                if(ret == -1) {
                    PPDEBUG(ctx, conn.mod_res, "send request -1 failed");
                    break;
                }
                else{
                    PPDEBUG(ctx, conn.mod_res, "send request success: cmd: %s", cmd);
                }         
                statemachine->stat = MOD_SNAPSHOT_STATUS_SUCCESS;
            }while(0);
            break;
        }
        case MOD_SNAPSHOT_STATUS_SUCCESS: {
            statemachine->stat = state_success;
            break;
        }
        case MOD_SNAPSHOT_STATUS_FAILED: {
            close(conn.ipc_sd);
            if(conn.mod_recv_data)
                free(conn.mod_recv_data);
            mod_snapshot_is_init = 0;
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