#ifndef _MAIN_H
#define _MAIN_H

#include <pthread.h>
#include <netinet/in.h>
#include "jansson.h"
#include "ev.h"

#define MCAST_GRP "239.255.0.1"
#define MCAST_PORT 5000
#define REPLY_PORT 5001
#define TCP_SOCKET_PORT 1357

typedef struct _ipc_header {
    int mod;
    int cmd;
    int len;
} ipc_header_t;

typedef struct _main_ctx {
    int multi_sockfd;
#ifdef CON_TCP
    int tcp_server_sockfd;
    int tcp_accept_sockfd;
    ev_io client_rd_io;
#endif
    struct ev_loop *loop;
    ev_io rd_io;
    ipc_header_t ipc_header;
    json_t *data;
    struct sockaddr_in remote_addr;
    void* statemachine;
    char mod_name[64];
    pthread_mutex_t lock;
    pthread_t multi_recv_thread;
} main_ctx;

#endif