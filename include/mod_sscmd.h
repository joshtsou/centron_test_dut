#ifndef _MOD_SSCMD_H
#define _MOD_SSCMD_H
#include "statemachine.h"
#include "mod.h"
#include <pthread.h>

#define MEDIA_SOCKET			"/tmp/ipc-media"
#define H1N1_SSCMD_MAGIC_SYNC	0x12345678
#define VIDEO_SOURCE_CHANNEL_NUMBER 4
#define CHT_VIDEO_SOURCE_STREAM_NUMBER 2
#define MAX_CONNECTION_NUMBER (VIDEO_SOURCE_CHANNEL_NUMBER * CHT_VIDEO_SOURCE_STREAM_NUMBER)
#define POST_BUFFER 10
#define BATCH_LENGTH 270

struct h1n1_sscmd_header
{
    int sync;	// magic sync information
    int cmd;	// command follows H1N1_SSCMD
    int length; // data length not including size of this header
};

typedef enum
{
    CMD_EX_PLAY_DATA_NC_STATUS=0,
    CMD_EX_PLAY_DATA_REC_MODE
} CMD_EX_PLAY_DATA_IDX;

typedef enum
{
    MOD_SSCMD_STATUS_START = MOD_SSCMD_IDX*MOD_STATUS_GROUP_NUM,
    MOD_SSCMD_STATUS_RECV_THREAD,
    MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP,
    MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY,
    MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP,
    MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE,
    MOD_SSCMD_STATUS_SUCCESS,
    MOD_SSCMD_STATUS_FAILED
} MOD_SSCMD_STATUS;

typedef enum
{
    H1N1_SSCMD_ACK,
    H1N1_SSCMD_MEDIA_SDP,
    H1N1_SSCMD_MEDIA_PLAY,
    H1N1_SSCMD_MEDIA_STOP,
    H1N1_SSCMD_MEDIA_IDX,	// ss to h1n1 client
    H1N1_SSCMD_DISCONN,		// notify the NC is disconnected
    H1N1_SSCMD_RECONN,		// notify the NC is reconnected
    H1N1_SSCMD_SDP_ACK,		// ss to h1n1 client
    H1N1_SSCMD_IFRAME_ACK,  // h1n1 to ss
    H1N1_SSCMD_STOP,  // h1n1 to ss
    H1N1_SSCMD_START,  // h1n1 to ss
    H1N1_SSCMD_REL_VID_FRAME_ACK,
    H1N1_SSCMD_REL_AUD_FRAME_ACK,
    H1N1_SSCMD_RESET,
    // new commands for resending
    H1N1_SSCMD_MEDIA_EX_PLAY,
    // not really stop stream that depends on nc state
    H1N1_SSCMD_MEDIA_EX_STOP,
    H1N1_SSCMD_MEDIA_NC_STATE,
    H1N1_SSCMD_MEDIA_RSDVD,
    H1N1_SSCMD_MEDIA_ACK_FAIL,
    H1N1_SSCMD_MEDIA_ACK_TIMEOUT,
    H1N1_SSCMD_MEDIA_ACK_EX_STOP,
    H1N1_SSCMD_MAX
} H1N1_SSCMD;

typedef enum
{
    H1N1_VIDEO_EVENTKEY_CAM_NONE,
    H1N1_VIDEO_EVENTKEY_CAM_VIDEO,
    H1N1_VIDEO_EVENTKEY_CAM_SAVE1,	// Full time video
    H1N1_VIDEO_EVENTKEY_CAM_SAVE2,	// Full time audio
    H1N1_VIDEO_EVENTKEY_CAM_RSDVD,
    H1N1_VIDEO_EVENTKEY_CAM_RSDAU,
    H1N1_VIDEO_EVENTKEY_CAM_AIRVD,
    H1N1_VIDEO_EVENTKEY_CAM_AIRAU,
    H1N1_VIDEO_EVENTKEY_CAM_FILEV,
    H1N1_VIDEO_EVENTKEY_CAM_FILEA,
    H1N1_VIDEO_EVENTKEY_CAM_LIVE,		// Defined by us for live streaming
} H1N1_VIDEO_EVENTKEY;

struct h1n1_sscmd_media_sdp
{
    unsigned short channel;
    unsigned short stream;
    unsigned short profile_index;
    char session_name[32];	// assigned session name of sdp
};

struct h1n1_sscmd_media_stream
{
    H1N1_VIDEO_EVENTKEY type;	//sch or event
    int connected;
    time_t event_time;
    time_t movie_time;
    int batch_length;	//seconds > 60
    char camid[33];
    int local_rec;
};

struct h1n1_sscmd_media_idx
{
    int type;
    int length;
    unsigned int offset;
    unsigned int ttm;
    int keyframe;
    struct timeval timestamp;
};

int mod_sscmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed); 

#endif