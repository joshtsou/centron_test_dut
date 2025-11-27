#include <stdlib.h>
#include <unistd.h>
#include "main.h"
#include "mod_ipc.h"
#include "socket.h"
#include "mod.h"

int mod_result_send(main_ctx *ctx, char *str) {
	ipc_header_t header;
	header.mod = ctx->ipc_header.mod;
	header.cmd = ctx->ipc_header.cmd;
	header.len = strlen(str);
	int ret = -1;
	do {
		if(socket_mcast_reply(ctx->multi_sockfd, REPLY_PORT, &header, sizeof(ipc_header_t), &ctx->remote_addr) == -1) {
			break;
		}
		if(socket_mcast_reply(ctx->multi_sockfd, REPLY_PORT, str, strlen(str), &ctx->remote_addr) == -1) {
			break;
		}
		ret = 0;
	}while(0);
	return ret;
}

void mod_result_append(char *mod_name, char *input, int input_size, char *target, int target_size) {
	char header[64] = "\0";
	char tail[64] = "\n";
    sprintf(header, "\n[MOD: %s RESULT] ", mod_name);
	do {
		if(!input || !target) {
			PDEBUG("test result append error.");
			break;
		}
		int remain_size = target_size - strlen(target) - strlen(header) - strlen(tail) - 1;
		if(remain_size < input_size) {
			PDEBUG("test result append error: no space.");
			break;
		}
		strncat(target, header, remain_size);
		strncat(target, input, remain_size);
		strncat(target, tail, remain_size);
	}while(0);
}
