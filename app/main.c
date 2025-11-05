#include <stdio.h>
#include <stdlib.h>
#include "ev.h"
#include "event.h"
#include "jansson.h"
#include "statemachine.h"
#include "socket.h"
#include "main.h"

#ifndef PDEBUG
#define PDEBUG(fmt, ...) do { \
        printf("[%s:%d] "fmt"\n",__FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0);
#endif

#define MCAST_GRP "239.255.0.1"
#define MCAST_PORT 5000

static void cmd_read_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
    main_ctx *ctx = (main_ctx*)ev_userdata(loop);
    char *buf;
    do {
        //read header
        if(socket_mcast_recvfrom(w->fd, &ctx->ipc_header, sizeof(ipc_header_t), &ctx->remote_addr) == -1)
            break;
        buf = calloc(ctx->ipc_header.len, 1);
        if(socket_mcast_recvfrom(w->fd, ) == -1)
            break;

        

    } while(0);
    if(buf)
        free(buf);
}

int init_main(main_ctx *ctx) {
    int ret = -1;
    do {
        ctx->multi_sockfd = socket_mcast_bind_group(MCAST_GRP, MCAST_PORT);
        if(ctx->multi_sockfd == -1) {
            PDEBUG("multi sock bind group error.");
            break;
        }
        pthread_mutex_init(&ctx->lock, NULL);
        //pthread_mutex_lock(&lock);
        //pthread_mutex_unlock(&lock);
        {
            ctx->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);
            ev_io_init(&ctx->rd_io, cmd_read_callback, ctx->multi_sockfd, EV_READ);
            ev_io_start(ctx->loop, &ctx->rd_io);
            ev_set_userdata(ctx->loop, &ctx);
            ev_run(ctx->loop, 0);
        }
        ret = 0;
    }while(0);
    return ret;
}

int main() {
    main_ctx ctx = {0};
    statemachine_t statemachine;
    do {
        if(init_main(&ctx) == -1) break;
        statemachine_init(&statemachine, &ctx);
        statemachine_main(&statemachine);
    }while(0);
}