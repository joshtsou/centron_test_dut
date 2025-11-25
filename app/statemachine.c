#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include "statemachine.h"
#include "mod_sscmd.h"
#include "mod_ptzcmd.h"
#include "mod_snapshot.h"
#include "mod_audio_playback.h"
#include "mod_ccmd.h"
#include "main.h"

bool isRun;

void statemachine_init(statemachine_t *statemachine, void *data) {
    statemachine->stat = STATEMACHINE_START;
    statemachine->data = data;
    isRun = true;
}

int statemachine_run(statemachine_t *statemachine) {
    // main_ctx *pctx = (main_ctx*)statemachine->data;
    // char res_message[1024];
    switch(statemachine->stat) {
        case STATEMACHINE_START:
            PDEBUG("[STATE]: STATEMACHINE_START");
            statemachine->stat = STATEMACHINE_SLEEP;
            break;
        case STATEMACHINE_SLEEP:
            PDEBUG("[STATE]: STATEMACHINE_SLEEP");
            usleep(500000);
            break;
        case STATEMACHINE_SUCCESS:
            //PPDEBUG(pctx, res_message, "test finished, success");
            isRun = false;
            break;
        case STATEMACHINE_FAILED:
            //PPDEBUG(pctx, res_message, "test finished, failed");
            isRun = false;
            break;
        default:
            return 0;
    }
    return 1;
}

void statemachine_main(statemachine_t *statemachine) {
    do {
        if(statemachine_run(statemachine)) continue;
        if(mod_sscmd_handler_run(statemachine, STATEMACHINE_SLEEP, STATEMACHINE_FAILED)) continue;
        if(mod_ccmd_handler_run(statemachine, STATEMACHINE_SLEEP, STATEMACHINE_FAILED)) continue;
        if(mod_ptzcmd_handler_run(statemachine, STATEMACHINE_SLEEP, STATEMACHINE_FAILED)) continue;
        if(mod_snapshot_handler_run(statemachine, STATEMACHINE_SLEEP, STATEMACHINE_FAILED)) continue;
        if(mod_audio_playback_handler_run(statemachine, STATEMACHINE_SLEEP, STATEMACHINE_FAILED)) continue;
    }while(isRun);
}