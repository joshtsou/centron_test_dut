#ifndef _STATEMACHINE_H
#define _STATEMACHINE_H

enum {
    STATEMACHINE_START = 0,
    STATEMACHINE_SLEEP,
    STATEMACHINE_SUCCESS,
    STATEMACHINE_FAILED
};

typedef struct _statemachine {
    int stat;
    void *data;
} statemachine_t;

void statemachine_init(statemachine_t *statemachine, void *data);
void statemachine_main(statemachine_t *statemachine);

#endif