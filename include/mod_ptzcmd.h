#ifndef _MOD_PTZ_H
#define _MOD_PTZ_H
#include "statemachine.h"
#include "mod.h"
#include <pthread.h>

#define PTZ_SOCKET			"/tmp/ipc-ptz"
#define SYSTEMD_COMMAND_PACKET_SYNC_CODE   0xffffffff

typedef struct {
	uint32_t sync;
	uint32_t cmd;
	uint32_t len;
} system_command_packet_t;

enum {
    MOD_PTZ_CMD_SET_PARAM_DATA_CHANNEL = 0,
    MOD_PTZ_CMD_SET_PARAM_DATA_ADDR,
    MOD_PTZ_CMD_SET_PARAM_DATA_P_SPEED,
    MOD_PTZ_CMD_SET_PARAM_DATA_T_SPEED,
    MOD_PTZ_CMD_SET_PARAM_DATA_Z_SPEED
};

enum {
    MOD_PTZ_CMD_MOVE_DATA_CHANNEL = 0,
    MOD_PTZ_CMD_MOVE_DATA_DIRECTION
};

enum {
    MOD_PTZ_CMD_ZOOM_DATA_CHANNEL = 0,
    MOD_PTZ_CMD_ZOOM_DATA_INOUT
};

enum {
    MOD_PTZ_CMD_FOCUS_DATA_CHANNEL = 0,
    MOD_PTZ_CMD_FOCUS_DATA_PLUS_MINUS
};

struct h1n1_ptzcmd_header
{
    int sync;	// magic sync information
    int cmd;	// command follows H1N1_SSCMD
    int length; // data length not including size of this header
};

typedef enum
{
    MOD_PTZCMD_STATUS_START = MOD_PTZ_IDX*MOD_STATUS_GROUP_NUM,
    MOD_PTZCMD_STATUS_PARAMETER_SETTING,
    MOD_PTZCMD_STATUS_MOVE,
    MOD_PTZCMD_STATUS_ZOOM,
    MOD_PTZCMD_STATUS_FOCUS,
    MOD_PTZCMD_STATUS_SUCCESS,
    MOD_PTZCMD_STATUS_FAILED
} MOD_PTZCMD_STATUS;

int mod_ptzcmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed);

#endif