#ifndef _MAIN_H
#define _MAIN_H

#include "mod_ccmd.h"
#include "mod_sscmd.h"
#include "jansson.h"
#include <pthread.h>
#include "ev.h"

enum {
    MOD_SSCMD_IDX = 1,
    MOD_CCMD_IDX
};

typedef struct _ipc_header {
    int mod;
    int cmd;
    int len;
} ipc_header_t;

typedef struct _main_ctx {
    int multi_sockfd;
    struct ev_loop *loop;
    ev_io rd_io;
    ipc_header_t ipc_header;
    json_t data;
    struct sockaddr_in remote_addr;
    pthread_mutex_t lock;
} main_ctx;

#endif