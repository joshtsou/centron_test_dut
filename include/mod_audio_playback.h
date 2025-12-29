#ifndef _MOD_AUDIO_PLAYBACK_H
#define _MOD_AUDIO_PLAYBACK_H
#include "statemachine.h"
#include "mod.h"

#define PLAYBACK_SOCKET			"/tmp/ipc-aout-0-0"
#define PLAY_BACK_AUDIO_PATH    "./scale_10s_stereo_16k_16bit.wav"
#define IPCMSG_TYPE_REQUEST_AUDIO_OUT           0x0001
#define IPCMSG_TYPE_RESPONSE_CODE               0x8fff

typedef struct audio_attr {
//	char dev_node[64]; /**< device node */
	uint32_t format; /**< bit width */
	uint32_t channels; /**< sound track */
	uint32_t rate; /**< sample rate */
//	unsigned long period_frames; /**< sample frames */
} audio_attr;

typedef struct ipcmsg_hdr {
    unsigned short type;
    unsigned short length;
} ipcmsg_hdr_t;

typedef struct ipcmsg_resp_code {
    unsigned int code;
} ipcmsg_resp_code_t;

enum {
    IPCMSG_RESP_CODE_OK = 0,
    IPCMSG_RESP_CODE_FAIL,
    IPCMSG_RESP_CODE_NOMEM,
    IPCMSG_RESP_CODE_FORMAT_ERR,
    IPCMSG_RESP_CODE_BUTT,
    IPCMSG_RESP_CODE_TOOMANY
};

typedef enum
{
    MOD_AUDIO_PLAYBACK_STATUS_START = MOD_AUDIO_PLAYBACK_IDX*MOD_STATUS_GROUP_NUM,
    MOD_AUDIO_PLAYBACK_STATUS_CHECK,
    MOD_AUDIO_PLAYBACK_STATUS_SEND,
    MOD_AUDIO_PLAYBACK_STATUS_SUCCESS,
    MOD_AUDIO_PLAYBACK_STATUS_FAILED
} MOD_AUDION_PLAYBACK_STATUS;

enum {
    MOD_AUDIO_PLAYBACK_CMD_SET_PARAM_DATA_TIMES = 0,
};

int mod_audio_playback_handler_run(statemachine_t *statemachine, int state_success, int state_failed);

#endif