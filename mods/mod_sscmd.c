#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "jansson.h"
#include "ev.h"
#include "main.h"
#include "mod_sscmd.h"

static int mod_sscmd_is_init = 0;
static conn_t conn[MAX_CONNECTION_NUMBER] = {0};

int mod_sscmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_conn_err = 0;
    switch(statemachine->stat) {
        case MOD_SSCMD_STATUS_START:
            do {
                if(mod_sscmd_is_init)
                    break;
                //connect ss
                for(int ch=0; ch<VIDEO_SOURCE_CHANNEL_NUMBER; ch++) {
                    for(int s=0; s<CHT_VIDEO_SOURCE_STREAM_NUMBER; s++) {
                        int idx = ch*CHT_VIDEO_SOURCE_STREAM_NUMBER+s;
                        conn[idx].channel = ch;
                        conn[idx].stream = s;
                        conn[idx].ipc = IPC_Create_Client(MEDIA_SOCKET);
                        if(!conn[idx].ipc) {
                            PDEBUG("MOD SSCMD CONNECT IPC ERROR, channel: %d, stream: %d", ch, s);
                            is_conn_err |= 1;
                        }
                        else {
                            TDEBUG("MOD SSCMD IPC CONNECT SUCCESS, channel: %d, stream: %d", ch, s);
                        }
                    }
                }
            }while(0);
            if(is_conn_err) {
                statemachine->stat = MOD_SSCMD_STATUS_START + ctx->ipc_header.cmd;
            }
            else {
                mod_sscmd_is_init = 1;
                statemachine->stat = MOD_SSCMD_STATUS_START + ctx->ipc_header.cmd;
            }
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_SDP");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_PLAY");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_EX_STOP");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE:
            TDEBUG("MOD_SSCMD_STATUS_H1N1_SSCMD_MEDIA_NC_STATE");
            statemachine->stat = MOD_SSCMD_STATUS_SUCCESS;
            break;
        case MOD_SSCMD_STATUS_SUCCESS:
            TDEBUG("MOD_SSCMD_STATUS_SUCCESS");
            statemachine->stat = state_success;
            break;
        case MOD_SSCMD_STATUS_FAILED:
            TDEBUG("MOD_SSCMD_STATUS_FAILED");
            statemachine->stat = state_failed;
            break;
        default:
            return 0;
            break;
    }
    return 1;
}

