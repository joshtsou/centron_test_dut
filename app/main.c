#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "ev.h"
#include "event.h"
#include "jansson.h"
#include "statemachine.h"
#include "socket.h"
#include "main.h"
#include "mod_ccmd.h"
#include "mod_sscmd.h"

static main_ctx g_ctx = {0};

static void cmd_read_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
    main_ctx *ctx = (main_ctx*)ev_userdata(loop);
    //main_ctx *ctx = (main_ctx*)w->data;
    char *buf=NULL;
    json_error_t error;
    do {
        if(socket_mcast_recvfrom(w->fd, &ctx->ipc_header, sizeof(ipc_header_t), &ctx->remote_addr) == -1)
            break;
        buf = calloc(1, ctx->ipc_header.len+1);
        if(socket_mcast_recvfrom(w->fd, buf, ctx->ipc_header.len, &ctx->remote_addr) == -1)
             break;
        TDEBUG("MOD: %d, CMD: %d", ctx->ipc_header.mod, ctx->ipc_header.cmd);
        TDEBUG("%s", buf);
        if((ctx->data = json_loads(buf, 0, &error)) == NULL) {
            PDEBUG("json string error. %s", error.text);
            break;
        }
        switch(ctx->ipc_header.mod) {
            case MOD_SSCMD_IDX: {
                statemachine_t *p_state = (statemachine_t*)ctx->statemachine;
                p_state->stat = MOD_SSCMD_STATUS_START;
                break;
            }
            default:
                TDEBUG("error: module not exist.");
                break;
        }
    } while(0);
    if(buf)
        free(buf);
}

void* recv_thread(void *arg) {
    main_ctx *ctx = (main_ctx*)arg;
    ctx->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);
    ev_io_init(&ctx->rd_io, cmd_read_callback, ctx->multi_sockfd, EV_READ);
    ev_io_start(ctx->loop, &ctx->rd_io);
    //ctx->rd_io.data = ctx;
    ev_set_userdata(ctx->loop, ctx);
    ev_run(ctx->loop, 0);
    return 0;
}

int init_main(main_ctx *ctx) {
    int ret = -1;
    do {
        ctx->statemachine = (statemachine_t*)calloc(1, sizeof(statemachine_t));
        if(!ctx->statemachine) {
            PDEBUG("statemachine_t calloc failed.");
            break;
        }
        ctx->multi_sockfd = socket_mcast_bind_group(MCAST_GRP, MCAST_PORT);
        if(ctx->multi_sockfd == -1) {
            PDEBUG("multi sock bind group error.");
            break;
        }
        pthread_mutex_init(&ctx->lock, NULL);
        pthread_create(&ctx->multi_recv_thread, NULL, recv_thread, ctx);
        pthread_detach(ctx->multi_recv_thread);
        //pthread_mutex_lock(&lock);
        //pthread_mutex_unlock(&lock);
        
        ret = 0;
    }while(0);
    return ret;
}

void exit_main() {
    if(g_ctx.data)
        json_decref(g_ctx.data);
    if(g_ctx.statemachine)
        free(g_ctx.statemachine);
}

void sigint_handler(int sig) {
    exit_main();
    exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);
    do {
        if(init_main(&g_ctx) == -1) break;
        statemachine_init(g_ctx.statemachine, &g_ctx);
        statemachine_main(g_ctx.statemachine);
    }while(0);
    exit_main();
    return 0;
}