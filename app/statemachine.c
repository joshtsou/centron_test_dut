#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include "statemachine_mod_handler.h"

bool isRun;

void statemachine_init(statemachine_t *statemachine, void *data) {
    statemachine->stat = STATEMACHINE_START;
    statemachine->data = data;
    isRun = true;
}

int statemachine_run(statemachine_t *statemachine, int state_success, int state_failed) {
    switch(statemachine->stat) {
        case STATEMACHINE_START:
            printf("[STATE]: STATEMACHINE_START\n");
            statemachine->stat = statemachine_mod_handler_get_start();
            break;
        case STATEMACHINE_SLEEP:
            printf("[STATE]: STATEMACHINE_SLEEP\n");
            usleep(500000);
            break;
        case STATEMACHINE_SUCCESS:
            printf("[STATE]: STATEMACHINE_SUCCESS\n");
            isRun = false;
            break;
        case STATEMACHINE_FAILED:
            printf("[STATE]: STATEMACHINE_FAILED\n");
            isRun = false;
            break;
        default:
            return 0;
    }
    return 1;
}

void statemachine_main(statemachine_t *statemachine) {
    do {
        if(statemachine_run(statemachine, STATEMACHINE_SUCCESS, STATEMACHINE_FAILED)) continue;
        if(statemachine_mod_handler_run(statemachine, STATEMACHINE_SUCCESS, STATEMACHINE_FAILED)) continue;
    }while(isRun);
}