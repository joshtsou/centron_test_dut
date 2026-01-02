#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdbool.h>
#include "main.h"
#include "mod_ccmd.h"
#include "mod_ipc.h"
#include "mod.h"

typedef struct connection {
    int ipc_fd;
    ev_io rd_io;
    char mod_request_str[2048];
	char mod_res[1024];
    char *mod_recv_data;
    pthread_t ipc_pthread;
    main_ctx *ctx;
} conn_t;

char *param_file_path[] = {
    "./videosource.json",
    "./audiosource.json",
    "./ptz.json",
    "./storage.json"
};

static conn_t conn = {0};
static bool thread_is_loop;

int mod_ccmd_parse_param_json(main_ctx *ctx, conn_t *conn, char *path) {
    int ret = -1;
    struct stat st;
    FILE *fp = NULL;
    char *buf = NULL;
    json_t *root = NULL;
    struct h1n1_ccmd_header hdr = { .sync = H1N1_CCMD_MAGIC, .cmd = H1N1_CCMD_REQ_PUSH_PARAM, .length = 0 };

    do {
        if (stat(path, &st) != 0) {
            PDEBUG("stat failed: %s", path);
            break;
        }
        fp = fopen(path, "rb");
        if (!fp) {
            PDEBUG("fopen failed: %s\n", path);
            break;
        }
        buf = calloc(1, st.st_size + 1);
        if (!buf) {
            PDEBUG("calloc failed");
            break;
        }
        if (fread(buf, 1, st.st_size, fp) != (size_t)st.st_size) {
            PDEBUG("fread failed");
            break;
        }
        root = json_loads(buf, 0, NULL);
        if (!root) {
            PDEBUG("json_loads failed");
            break;
        }
        hdr.length = st.st_size + 1;
        PPDEBUG(ctx, conn->mod_res, "read json file ok, path: %s", path);
        if((ret = write(conn->ipc_fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
            PPDEBUG(ctx, conn->mod_res, "push param failed, send header faild");
            break;
        }
        if((ret = write(conn->ipc_fd, buf, hdr.length)) != hdr.length) {
            PPDEBUG(ctx, conn->mod_res, "push param failed, send data failed");
            break;
        }
        ret=0;
    }while(0);

    if (fp) fclose(fp);
    if (buf) free(buf);
    if (root) json_decref(root);
    return ret;
}

int mod_ccmd_socket_connect(conn_t *conn) {
    int ret = -1;
    struct sockaddr_un un;

    if(conn->ipc_fd != 0) {
        close(conn->ipc_fd);
        conn->ipc_fd = 0;
    }
    do {
        conn->ipc_fd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (conn->ipc_fd == -1) {
            PDEBUG("create socket fail");
            break;
        }

        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, CCMD_SOCKET);

        if (connect(conn->ipc_fd, (struct sockaddr*)&un, sizeof(un)) != 0) {
            close(conn->ipc_fd);
            conn->ipc_fd = 0;
            PDEBUG("socket connection error");
            break;
        }
        ret = 0;
    }while(0);
    return ret;
}

int ipc_ccmd_request_recv(conn_t *conn, main_ctx *ctx) {
    int ret = -1;
    struct h1n1_ccmd_header hdr = {0};
    char *data = NULL;
    statemachine_t *pstat = ctx->statemachine;
    do {
        if((ret = recv(conn->ipc_fd, &hdr, sizeof(hdr), MSG_WAITALL)) != sizeof(hdr)) {
            PPDEBUG(ctx, conn->mod_res, "recv header error");
            break;
        }
        data = (char*)calloc(hdr.length, 1);
        if(!data) {
            PPDEBUG(ctx, conn->mod_res, "calloc data space error");
            break;
        }
        if((ret = recv(conn->ipc_fd, data, hdr.length, MSG_WAITALL) != hdr.length)) {
            PPDEBUG(ctx, conn->mod_res, "recv data error");
            break;
        }
        switch(hdr.cmd) {
            case H1N1_CCMD_REQ_VIDEO_REPLY:
                PPDEBUG(ctx, conn->mod_res, "recv cmd H1N1_CCMD_REQ_VIDEO_REPLY, data: %s", data);
                pstat->stat = MOD_CCMD_STATUS_SUCCESS;
                ret = 0;
                break;
            case H1N1_CCMD_REQ_PUSH_REPLY:
                PPDEBUG(ctx, conn->mod_res, "recv cmd H1N1_CCMD_REQ_PUSH_REPLY, data: %s", data);
                pstat->stat = MOD_CCMD_STATUS_SUCCESS;
                ret = 0;
                break;
            default:
                pstat->stat = MOD_CCMD_STATUS_FAILED;
                break;
        }        
    }while(0);
    return ret;
}

static void ccmd_reciever_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
	conn_t *con = (conn_t*)w->data;
	main_ctx *ctx = (main_ctx*)ev_userdata(loop);
	//int ret = 0;
	//PPDEBUG(ctx, con->mod_res, "PTZCMD RECVing ... active_fd: %d", w->fd);
    if(ipc_ccmd_request_recv(con, ctx) != 0) {
        PPDEBUG(ctx, con->mod_res, "recv error.");
    }
    else {
        PPDEBUG(ctx, con->mod_res, "recv success");
    }
    close(w->fd);
    con->ipc_fd = 0;
    ev_io_stop(loop, w);
}

void *ccmd_ipc_recv_thread(void *arg)
{
    struct timeval tv;
    conn_t *conn = (conn_t*)arg;
    main_ctx *ctx = conn->ctx;
    //statemachine_t *pstat = ctx->statemachine;
    fd_set readfds;
    int ret = 0;
    do {
        FD_ZERO(&readfds);
        FD_SET(conn->ipc_fd, &readfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        ret = select(conn->ipc_fd+1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(conn->ipc_fd, &readfds)) {
            if(ipc_ccmd_request_recv(conn, ctx) != 0) {
                PPDEBUG(ctx, conn->mod_res, "recv error.");
            }
            else {
                PPDEBUG(ctx, conn->mod_res, "recv success");
            }
            break;
        } else if(ret == -1){
            TDEBUG("ptz module ipc select error");
            break;
        }
    }while(thread_is_loop);
    return 0;
}

int mod_ccmd_reciever_create(conn_t *conn, main_ctx *ctx) {
    if(!conn->ipc_fd)
	{
		PDEBUG("%s fail due to ev_io init", ctx->mod_name);
		return -1;
	}
	conn->rd_io.data = conn;
    ev_io_init(&conn->rd_io, ccmd_reciever_io_callback, conn->ipc_fd, EV_READ);
    ev_io_start(ctx->loop, &conn->rd_io);
    return 0;
}

int mod_ccmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_err = 0;

    switch(statemachine->stat) {
        case MOD_CCMD_STATUS_START: {
            do {
                sprintf(ctx->mod_name, "ccmd");
                if(conn.ipc_fd)
                    break;
                if(mod_ccmd_socket_connect(&conn)!=0) {
                    PPDEBUG(ctx, conn.mod_res, "socket connection failed");
                    is_err = 1;
                }
            }while(0);
            if(is_err) {
                statemachine->stat = MOD_CCMD_STATUS_FAILED;
            }
            else {
                // if(!ev_is_active(&conn.rd_io)) {
                //     PDEBUG("recv thread created");
                //     mod_ccmd_reciever_create(&conn, ctx);
                // }
                conn.ctx = ctx;
                pthread_create(&conn.ipc_pthread, NULL, ccmd_ipc_recv_thread, &conn);
                pthread_detach(conn.ipc_pthread);
                statemachine->stat = MOD_CCMD_STATUS_START + ctx->ipc_header.cmd;
            }
            //statemachine->stat = MOD_CCMD_STATUS_START + ctx->ipc_header.cmd;
            break;
        }
        case MOD_CCMD_STATUS_REQ_VIDEO: {
            struct h1n1_ccmd_header hdr = { .sync = H1N1_CCMD_MAGIC, .cmd = H1N1_CCMD_REQ_VIDEO, .length = 0 };
            int ret = write(conn.ipc_fd, &hdr, sizeof(hdr));
            if(ret != sizeof(hdr)) {
                PPDEBUG(ctx, conn.mod_res, "request video error, send header failed");
                statemachine->stat = MOD_CCMD_STATUS_FAILED;
            }
            else {
                statemachine->stat = STATEMACHINE_SLEEP;
            }
            break;
        }
        case MOD_CCMD_STATUS_PUSH_PARAM: {
            statemachine->stat = MOD_CCMD_STATUS_FAILED;
            int param_idx = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_CCMD_CMD_PUSH_PARAM_DATA_IDX), "value")));
            do {
                if(param_idx < MOD_CCMD_PARAM_JSON_IDX_VIDEOSOURCE || param_idx >= MOD_CCMD_PARAM_JSON_IDX_END) {
                    break;
                }
                if(mod_ccmd_parse_param_json(ctx, &conn, param_file_path[param_idx]) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "send request failed");
                    break;
                }
                PPDEBUG(ctx, conn.mod_res, "test success");
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_CCMD_STATUS_SUCCESS: {
            PPDEBUG(ctx, conn.mod_res, "test finished, success");
            thread_is_loop = 0;
            close(conn.ipc_fd);
            conn.ipc_fd = 0;
            statemachine->stat = state_success;
            break;
        }
        case MOD_CCMD_STATUS_FAILED: {
            PPDEBUG(ctx, conn.mod_res, "test finished, failed");
            thread_is_loop = 0;
            close(conn.ipc_fd);
            conn.ipc_fd = 0;
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