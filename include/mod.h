#ifndef _MOD_H
#define _MOD_H
#include "main.h"

#ifndef PPDEBUG
#define PPDEBUG(ctx, buf, fmt, ...) \
do { \
    char temp[1024] = "\0"; \
    sprintf(temp, fmt, ##__VA_ARGS__); \
    mod_result_append(ctx->mod_name, temp, strlen(temp), buf, sizeof(buf)); \
    mod_result_send(ctx, buf); \
} while(0);
#endif


void mod_result_send(main_ctx *ctx, char *str);
void mod_result_append(char *mod_name, char *input, int input_size, char *target, int target_size);

#endif