#include <stdio.h>
#include <stdlib.h>
#include "ev.h"
#include "event.h"
#include "jansson.h"
#include "statemachine.h"
#include "statemachine_mod_handler.h"
#include "socket.h"

mods_ctx ctx = {0};

void init_main() {
    
}

int main() {
    statemachine_t statemachine;
    init_main();
    statemachine_init(&statemachine, &ctx);
    statemachine_main(&statemachine);
}