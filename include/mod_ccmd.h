#ifndef _MOD_CCMD_H
#define _MOD_CCMD_H
#include "statemachine.h"
#include "mod.h"

#define CCMD_SOCKET			"/tmp/ipc-ccmd"
#define H1N1_CCMD_MAGIC     0x1357

struct h1n1_ccmd_header
{
        int sync;        // magic sync information
        int cmd;        // command follows H1N1_CCMD
        int length; // data length not including size of this header
};

typedef enum
{
    H1N1_CCMD_REQ_VIDEO,
    H1N1_CCMD_REQ_VIDEO_REPLY,
    H1N1_CCMD_REQ_PUSH_PARAM,
    H1N1_CCMD_REQ_PUSH_REPLY,
    H1N1_CCMD_END
} H1N1_CCMD;

typedef enum
{
    MOD_CCMD_STATUS_START = MOD_CCMD_IDX*MOD_STATUS_GROUP_NUM,
    MOD_CCMD_STATUS_REQ_VIDEO,
    MOD_CCMD_STATUS_PUSH_PARAM,
    MOD_CCMD_STATUS_SUCCESS,
    MOD_CCMD_STATUS_FAILED
} MOD_CCMD_STATUS;

enum {
    MOD_CCMD_CMD_PUSH_PARAM_DATA_IDX = 0,
};

enum {
    MOD_CCMD_PARAM_JSON_IDX_VIDEOSOURCE = 0,
    MOD_CCMD_PARAM_JSON_IDX_AUDIOSOURCE,
    MOD_CCMD_PARAM_JSON_IDX_PTZ,
    MOD_CCMD_PARAM_JSON_IDX_STORAGE,
    MOD_CCMD_PARAM_JSON_IDX_NETWORK,
    MOD_CCMD_PARAM_JSON_IDX_SERIAL,
    MOD_CCMD_PARAM_JSON_IDX_END
};

int mod_ccmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed);

#endif