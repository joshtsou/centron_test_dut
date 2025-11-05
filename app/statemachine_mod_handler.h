#ifndef _STATEMACHINE_MOD_HANDLER_H
#define _STATEMACHINE_MOD_HANDLER_H
#include "statemachine.h"
#include "ev.h"
#include "jansson.h"

#define FILE_MODS_JSON "/tmp/mods.json"

enum {
    STATEMACHINE_MOD_INIT = 10,
    STATEMACHINE_MOD_HADNLER_1,
    STATEMACHINE_MOD_HADNLER_2,
    STATEMACHINE_MOD_HADNLER_3,
    STATEMACHINE_MOD_HANDLER_SEND,
    STATEMACHINE_MOD_HADNLER_SUCCCESS,
    STATEMACHINE_MOD_HADNLER_FAILED
};

typedef struct _mods_ctx {
    int sockfd;
    int mod_idx;
    int cmd_idx;
    struct ev_loop *loop;
    ev_io rd_io;
    //ev_io wt_io;
    json_t *mods;
    json_t *cmds;
    json_t *selected_mod;
    json_t *selected_cmd;
    json_t *curr_data;
} mods_ctx;

typedef struct _ipc_header {
    int mod;
    int cmd;
    int len;
} ipc_header;

int statemachine_mod_handler_get_start();
int statemachine_mod_handler_run(statemachine_t *statemachine, int state_success, int state_failed);

#endif