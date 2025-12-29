#ifndef _MOD_H
#define _MOD_H
#include <unistd.h>
#include "main.h"
#include "debug.h"

#define MOD_STATUS_GROUP_NUM 10

enum {
    MOD_SSCMD_IDX = 1,
    MOD_CCMD_IDX,
    MOD_PTZ_IDX,
    MOD_SNAPSHOT_IDX,
    MOD_AUDIO_PLAYBACK_IDX
};

#ifndef PPDEBUG
#define PPDEBUG(ctx, buf, fmt, ...) \
do { \
    TDEBUG(fmt, ##__VA_ARGS__); \
    char temp[1024] = "\0"; \
    buf[0] = '\0'; \
    sprintf(temp, fmt, ##__VA_ARGS__); \
    mod_result_append(ctx->mod_name, temp, strlen(temp), buf, sizeof(buf)); \
    if(mod_result_send(ctx, buf) == -1) { \
        TDEBUG("send debug failed. message: %s", buf); \
    } \
} while(0);
#endif


int mod_result_send(main_ctx *ctx, char *str);
void mod_result_append(char *mod_name, char *input, int input_size, char *target, int target_size);

#endif