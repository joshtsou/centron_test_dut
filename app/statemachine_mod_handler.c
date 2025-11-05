#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "statemachine_mod_handler.h"
#include "socket.h"

#define MCAST_GRP "239.255.0.1"
#define MCAST_PORT 5000
#define LOCAL_PORT 5001

#ifndef PDEBUG
#define PDEBUG(fmt, ...) do { \
        printf("[%s:%d] "fmt"\n",__FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0);
#endif

int statemachine_mod_handler_get_digit() {
    char input[64] = {0};
    int target = 0;
    if (fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\n")] = '\0';

        for (size_t i = 0; i < strlen(input); i++) {
            if (!isdigit((unsigned char)input[i]))
                break;
            if(i == strlen(input)-1)
                target = atoi(input);
        }
    }
    return target;
}

char* statemachine_mod_handler_get_string(char *res) {
    do {
        if(!res) {
            PDEBUG("get string from stdin error");
            break;
        }
        res[0] = '\0';
        if (fgets(res, sizeof(res), stdin)) {
            res[strcspn(res, "\n")] = '\0';
        }
    }while(0);
    return res;
}

void statemachine_mod_handler_show_selection(json_t *j, int *select, char *key) {
    size_t index;
    json_t *value;
    size_t list_size = json_array_size(j);
    json_array_foreach(j, index, value) {
        printf("%ld) %s\n", index+1, json_string_value(json_object_get(value, key)));
    }
    *select = 0;
    do {
        printf("select: ");
        *select = statemachine_mod_handler_get_digit();
        if(*select > 0 && *select <= list_size)
            break;
    }while(1);
    printf("\n");
}

int statemachine_mod_handler_get_start() {
    return  STATEMACHINE_MOD_INIT;
}

static void statemachine_mod_handler_read_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
    ipc_header header;
    struct sockaddr_in remote_addr;
    mods_ctx *ctx = (mods_ctx*)w->data;
    char buf[64] = {0};
    do {
        if(socket_mcast_recvfrom(w->fd, &header, sizeof(ipc_header), &remote_addr) == -1) {
            PDEBUG("recv ipc error: recv header error");
            break;
        }
        if(header.mod != ctx->mod_idx) {
            PDEBUG("recv ipc error: module id not match.");
            break;
        }
        if(header.cmd != ctx->cmd_idx) {
            PDEBUG("recv ipc error: cmd id not match.");
            break;
        }
        if(socket_mcast_recvfrom(w->fd, buf, header.len, &remote_addr) == -1) {
            PDEBUG("recv ipc error: recv buf error");
            break;
        }
    } while(0);
    printf("[RECV FROM DUT]: %s\n", buf);
}

int statemachine_mod_handler_send(mods_ctx *ctx, char *val) {
    ipc_header hd;
    int ret = -1;
    do {
        if(!val) {
            PDEBUG("json dumps failed.");
            break;
        }
        hd.mod = ctx->mod_idx;
        hd.cmd = ctx->cmd_idx;
        hd.len = strlen(val);
        if(socket_mcast_sendto(ctx->sockfd, MCAST_GRP, MCAST_PORT, &hd, sizeof(ipc_header)) == -1) {
            PDEBUG("socket send failed.");
            break;
        }
        if(socket_mcast_sendto(ctx->sockfd, MCAST_GRP, MCAST_PORT, val, strlen(val)) == -1) {
            PDEBUG("socket send failed.");
            break;
        }
        ret = 0;
    } while(0);
    return ret;
}

int statemachine_mod_handler_init(statemachine_t *statemachine, mods_ctx *ctx) {
    int ret = -1;
    memset(ctx, 0, sizeof(mods_ctx));
    do {
        if((ctx->sockfd = socket_mcast_bind(LOCAL_PORT)) == -1)
            break;
        {
            ctx->loop = ev_loop_new(EVBACKEND_EPOLL | EVFLAG_NOENV);
            ev_io_init(&ctx->rd_io, statemachine_mod_handler_read_callback, ctx->sockfd, EV_READ);
            ev_io_start(ctx->loop, &ctx->rd_io);
            ctx->rd_io.data = ctx;
        }
        ctx->mods = json_object_get(json_load_file(FILE_MODS_JSON, 0, NULL), "mods");
        if(!ctx->mods) {
            PDEBUG("statemachine_mod_handler read mods file error.");
            break;
        }
        ret = 0;
    } while(0);
    return ret;
}

int statemachine_mod_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    mods_ctx *ctx = (mods_ctx*)statemachine->data;
    size_t index;
    json_t *value;
    switch(statemachine->stat) {
        case STATEMACHINE_MOD_INIT:
            printf("[STATE]: STATEMACHINE_MOD_INIT\n");
            if(statemachine_mod_handler_init(statemachine, ctx))
                statemachine->stat = STATEMACHINE_MOD_HADNLER_FAILED;
            else
                statemachine->stat = STATEMACHINE_MOD_HADNLER_1;
            break;
        case STATEMACHINE_MOD_HADNLER_1:
            printf("[TEST MODULES]:\n");
            statemachine_mod_handler_show_selection(ctx->mods, &ctx->mod_idx, "name");
            ctx->selected_mod = json_array_get(ctx->mods, ctx->mod_idx-1);
            statemachine->stat = STATEMACHINE_MOD_HADNLER_2;
            break;
        case STATEMACHINE_MOD_HADNLER_2:
            //printf("[STATE]: STATEMACHINE_MOD_HADNLER_2\n");
            printf("[MODULE %s COMMANDS]\n", json_string_value(json_object_get(ctx->selected_mod, "name")));
            ctx->cmds = json_object_get(ctx->selected_mod, "cmds");
            statemachine_mod_handler_show_selection(ctx->cmds, &ctx->cmd_idx, "name");
            ctx->selected_cmd = json_array_get(ctx->cmds, ctx->cmd_idx-1);
            statemachine->stat = STATEMACHINE_MOD_HADNLER_3;
            break;
        case STATEMACHINE_MOD_HADNLER_3:
            printf("[MODULE: %s, COMMAND: %s Data]\n", 
                json_string_value(json_object_get(ctx->selected_mod, "name")),
                json_string_value(json_object_get(ctx->selected_cmd, "name")));
            if(ctx->curr_data != NULL)
                json_decref(ctx->curr_data);
            ctx->curr_data = json_deep_copy(json_object_get(ctx->selected_cmd, "datas"));
            json_array_foreach(ctx->curr_data, index, value) {
                printf("%ld) %s\n",index+1, json_string_value(json_object_get(value, "name")));
                char str_val[64];
                json_object_set_new(value, "value", json_string(statemachine_mod_handler_get_string(str_val)));
            }
            // json_array_foreach(ctx->curr_data, index, value) {
            //     printf("%ld) value: %s\n", index+1, json_string_value(json_object_get(value, "value")));
            // }
            statemachine->stat = STATEMACHINE_MOD_HANDLER_SEND;
            break;
        case STATEMACHINE_MOD_HANDLER_SEND:
        {
            char *val = json_dumps(ctx->curr_data, 0);
            if(statemachine_mod_handler_send(ctx, val) == 0) {
                statemachine->stat = STATEMACHINE_MOD_HADNLER_SUCCCESS;
            }
            else {
                statemachine->stat = STATEMACHINE_MOD_HADNLER_FAILED;
            }
            free(val);
            break;
        }
        case STATEMACHINE_MOD_HADNLER_SUCCCESS:
            if(ctx->mods)
                json_decref(ctx->mods);
            if(ctx->curr_data)
                json_decref(ctx->curr_data);
            statemachine->stat = state_success;
            break;
        case STATEMACHINE_MOD_HADNLER_FAILED:
            if(ctx->mods)
                json_decref(ctx->mods);
            if(ctx->curr_data)
                json_decref(ctx->curr_data);
            statemachine->stat = state_failed;
            break;
        default:
            return 0;
    }
    return 1;
}