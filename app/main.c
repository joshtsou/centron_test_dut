#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include "ev.h"
#include "event.h"
#include "jansson.h"
#include "statemachine.h"
#include "socket.h"
#include "main.h"
#include "debug.h"
#include "mod.h"
#include "mod_sscmd.h"
#include "mod_ptzcmd.h"
#include "mod_snapshot.h"
#include "mod_audio_playback.h"
#include "mod_ccmd.h"

static main_ctx g_ctx = {0};

static void cmd_read_callback(struct ev_loop *loop, struct ev_io *w, int revents)
{
    main_ctx *ctx = (main_ctx *)ev_userdata(loop);
    char *buf = NULL;
    json_error_t error;
    if (ctx->data != NULL)
        free(ctx->data);
    do
    {
#ifndef CON_TCP
        if (socket_mcast_recvfrom(w->fd, &ctx->ipc_header, sizeof(ipc_header_t), &ctx->remote_addr) == -1)
        {
            TDEBUG("main recv wrong header from sender. error");
            break;
        }
        TDEBUG("MOD: %d, CMD: %d", ctx->ipc_header.mod, ctx->ipc_header.cmd);
        buf = calloc(1, ctx->ipc_header.len + 1);
        usleep(10000);
        if (socket_mcast_recvfrom(w->fd, buf, ctx->ipc_header.len, &ctx->remote_addr) == -1)
        {
            TDEBUG("main recv wrong data from sender. error");
            break;
        }
#else
        int ret = 0;
        ret = socket_tcp_recv(w->fd, &ctx->ipc_header, sizeof(ipc_header_t));
        if(ret == 0) {
            TDEBUG("socket disconnected.");
            ev_io_stop(loop, w);
            close(w->fd);
            exit(0);
            // statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            // p_state->stat = STATEMACHINE_FAILED;
            break;
        }
        if(ret != sizeof(ipc_header_t)){
            TDEBUG("main recv wrong header from sender. error");
            // statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            // p_state->stat = STATEMACHINE_FAILED;
            break;
        }
        usleep(20000);
        TDEBUG("MOD: %d, CMD: %d, LEN:%d", ctx->ipc_header.mod, ctx->ipc_header.cmd, ctx->ipc_header.len);
        buf = calloc(1, ctx->ipc_header.len + 1);
        if (socket_tcp_recv(w->fd, buf, ctx->ipc_header.len) != ctx->ipc_header.len)
        {
            TDEBUG("main recv wrong data from sender. error");
            break;
        }
#endif
        TDEBUG("RECV DATA: %s", buf);
        if ((ctx->data = json_loads(buf, 0, &error)) == NULL)
        {
            PDEBUG("json string error. %s", error.text);
            break;
        }
        switch (ctx->ipc_header.mod)
        {
        case MOD_SSCMD_IDX:
        {
            statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            p_state->stat = MOD_SSCMD_STATUS_START;
            break;
        }
        case MOD_CCMD_IDX:
        {
            statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            p_state->stat = MOD_CCMD_STATUS_START;
            break;
        }
        case MOD_PTZ_IDX:
        {
            statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            p_state->stat = MOD_PTZCMD_STATUS_START;
            break;
        }
        case MOD_SNAPSHOT_IDX:
        {
            statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            p_state->stat = MOD_SNAPSHOT_STATUS_START;
            break;
        }
        case MOD_AUDIO_PLAYBACK_IDX:
        {
            statemachine_t *p_state = (statemachine_t *)ctx->statemachine;
            p_state->stat = MOD_AUDIO_PLAYBACK_STATUS_START;
            break;
        }
        default:
            TDEBUG("error: module not exist.");
            break;
        }
    } while (0);
    if (buf)
        free(buf);
}

#ifdef CON_TCP
static void tcp_server_accept_callback(struct ev_loop *loop, struct ev_io *w, int revents)
{
    TDEBUG("new tcp connection coming...");
    main_ctx *ctx = (main_ctx *)ev_userdata(loop);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ctx->tcp_accept_sockfd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);
    int flags = fcntl(ctx->tcp_accept_sockfd, F_GETFL, 0);
    fcntl(ctx->tcp_accept_sockfd, F_SETFL, flags | O_NONBLOCK);
    ev_io_init(&ctx->client_rd_io, cmd_read_callback, ctx->tcp_accept_sockfd, EV_READ);
    ev_io_start(loop, &ctx->client_rd_io);
}
#endif

void *recv_thread(void *arg)
{
    main_ctx *ctx = (main_ctx *)arg;
    ctx->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);
#ifndef CON_TCP
    ev_io_init(&ctx->rd_io, cmd_read_callback, ctx->multi_sockfd, EV_READ);
#else
    ev_io_init(&ctx->rd_io, tcp_server_accept_callback, ctx->tcp_server_sockfd, EV_READ);
#endif
    ev_io_start(ctx->loop, &ctx->rd_io);
    // ctx->rd_io.data = ctx;
    ev_set_userdata(ctx->loop, ctx);
    ev_run(ctx->loop, 0);
    return 0;
}

int init_main(main_ctx *ctx)
{
    int ret = -1;
    do
    {
        ctx->statemachine = (statemachine_t *)calloc(1, sizeof(statemachine_t));
        if (!ctx->statemachine)
        {
            PDEBUG("statemachine_t calloc failed.");
            break;
        }
#ifndef CON_TCP
        ctx->multi_sockfd = socket_mcast_bind_group(MCAST_GRP, MCAST_PORT);
        if (ctx->multi_sockfd == -1)
        {
            PDEBUG("multi sock bind group error.");
            break;
        }
#else
        ctx->tcp_server_sockfd = socket_tcp_server_bind(TCP_SOCKET_PORT);
        if (ctx->tcp_server_sockfd == -1)
        {
            PDEBUG("tcp socket bind error.");
            break;
        }
#endif
        pthread_mutex_init(&ctx->lock, NULL);
        pthread_create(&ctx->multi_recv_thread, NULL, recv_thread, ctx);
        pthread_detach(ctx->multi_recv_thread);
        // pthread_mutex_lock(&lock);
        // pthread_mutex_unlock(&lock);

        ret = 0;
    } while (0);
    return ret;
}

void exit_main()
{
    if (g_ctx.data)
        json_decref(g_ctx.data);
    if (g_ctx.statemachine)
        free(g_ctx.statemachine);
    ev_break(g_ctx.loop, EVBREAK_ALL);
}

void sigint_handler(int sig)
{
    exit_main();
    exit(0);
}

int main()
{
    signal(SIGINT, sigint_handler);
    do
    {
        if (init_main(&g_ctx) == -1)
            break;
        statemachine_init(g_ctx.statemachine, &g_ctx);
        statemachine_main(g_ctx.statemachine);
    } while (0);
    exit_main();
    return 0;
}