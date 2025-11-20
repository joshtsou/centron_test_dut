#ifndef _MOD_SNAPSHOT_H
#define _MOD_SNAPSHOT_H
#include "statemachine.h"
#include "mod.h"

#define SNAPSHOT_SOCKET			"/tmp/ipc-snapshot"

typedef enum
{
    MOD_SNAPSHOT_STATUS_START = MOD_SNAPSHOT_IDX*MOD_STATUS_GROUP_NUM,
    MOD_SNAPSHOT_STATUS_GO,
    MOD_SNAPSHOT_STATUS_SUCCESS,
    MOD_SNAPSHOT_STATUS_FAILED
} MOD_SNAPSHOT_STATUS;

enum {
    MOD_SNAPSHOT_CMD_SET_PARAM_DATA_CHANNEL = 0,
    MOD_SNAPSHOT_CMD_SET_PARAM_DATA_WIDTH,
    MOD_SNAPSHOT_CMD_SET_PARAM_DATA_HEIGHT,
    MOD_SNAPSHOT_CMD_SET_PARAM_DATA_QUALITY
};

int mod_snapshot_handler_run(statemachine_t *statemachine, int state_success, int state_failed);

#endif