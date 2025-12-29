#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include "mod_ptzcmd.h"
#include "mod_ipc.h"
#include "socket.h"
#include "mod.h"

typedef struct connection {
    IPC_Socket *ipc;
    int channel;
    ev_io rd_io;
	char mod_res[1024];
    char mod_recv_data[1024];
    json_t *jrequest;
} conn_t;

static conn_t conn;
static int mod_ptzcmd_is_init = 0;
static char* direction_map[9] = {"left", "upleft", "up", "upright", "right", "downright", "down", "downleft", "home"};
static char* zoom_map[2] = {"in", "out"};
static char* focus_map[2] = {"minus", "plus"};

int ipc_ptz_request_recv(main_ctx* ctx, conn_t* con, char *result, int size) {
    int ret = -1;
    char *recv_val = NULL;
    system_command_packet_t hd;
    do {
        if(IPC_Recv(conn.ipc, (unsigned char *)&hd, sizeof(system_command_packet_t), 0) != sizeof(system_command_packet_t)) {
            PPDEBUG(ctx, con->mod_res, "recv header error.");
            break;
        }
        if(hd.cmd != 'j') {
            PPDEBUG(ctx, con->mod_res, "recv header cmd error");
            break;
        }
        if(hd.sync != SYSTEMD_COMMAND_PACKET_SYNC_CODE) {
            PPDEBUG(ctx, con->mod_res, "recv header sync error");
            break;
        }
        recv_val = calloc(1, hd.len+1);
        if(IPC_Recv(conn.ipc, (unsigned char *)recv_val, hd.len, 0) != hd.len) {
            PPDEBUG(ctx, con->mod_res, "recv data len error.");
            break;
        }
        snprintf(result, size, "%s", recv_val);
        ret = 0;
    }while(0);
    if(recv_val)
        free(recv_val);
    return ret;
}

int ipc_ptz_request_send(main_ctx *ctx, json_t *data) {
    int ret = -1;
    char *str_request = json_dumps(data, 0);
    system_command_packet_t hd;
    do {
        if(!str_request)
            break;
        hd.sync = SYSTEMD_COMMAND_PACKET_SYNC_CODE;
        hd.cmd = 'J';
        hd.len = strlen(str_request)+1;
        if(IPC_Send(conn.ipc, (unsigned char *)&hd, sizeof(hd)) != sizeof(hd)) {
            PDEBUG("send header error.");
            break;
        }
        if(IPC_Send(conn.ipc, (unsigned char *)str_request, hd.len) != hd.len) {
            PDEBUG("send data error");
            break;
        }
        PPDEBUG(ctx, conn.mod_res, "%s", str_request);
        ret = 0;
    }while(0);
    if(str_request)
        free(str_request);
    return ret;
}

static void ptzcmd_reciever_io_callback(struct ev_loop *loop, struct ev_io *w, int revents) {
	conn_t *con = (conn_t*)w->data;
	main_ctx *ctx = (main_ctx*)ev_userdata(loop);
    statemachine_t *pstat = ctx->statemachine;
	//int ret = 0;
	PPDEBUG(ctx, con->mod_res, "PTZCMD RECVing ... active_fd: %d", w->fd);
    if(ipc_ptz_request_recv(ctx, con, con->mod_recv_data, sizeof(con->mod_recv_data)) != 0) {
        PPDEBUG(ctx, con->mod_res, "recv error.");
        pstat->stat = MOD_PTZCMD_STATUS_FAILED;
    }
    else {
        PPDEBUG(ctx, con->mod_res, "%s", con->mod_recv_data);
        pstat->stat = MOD_PTZCMD_STATUS_SUCCESS;
    }
    close(w->fd);
    mod_ptzcmd_is_init = 0;
    IPC_Destroy(con->ipc);
    ev_io_stop(loop, w);
}

int mod_ptzcmd_reciever_create(conn_t *conn, main_ctx *ctx) {
    if(!IPC_SOCKET_CHECK(conn->ipc))
	{
		PDEBUG("%s fail due to ev_io init", ctx->mod_name);
		return -1;
	}
	conn->rd_io.data = conn;
    ev_io_init(&conn->rd_io, ptzcmd_reciever_io_callback, IPC_Select_Object(conn->ipc), EV_READ);
    ev_io_start(ctx->loop, &conn->rd_io);
    return 0;
}

int mod_ptzcmd_handler_run(statemachine_t *statemachine, int state_success, int state_failed) {
    main_ctx *ctx = (main_ctx*)statemachine->data;
    int is_err = 0;
    switch(statemachine->stat) {
        case MOD_PTZCMD_STATUS_START: {
            do {
                sprintf(ctx->mod_name, "ptzcmd");
                if(mod_ptzcmd_is_init)
                    break;
                conn.ipc = IPC_Create_Client(PTZ_SOCKET);
                if(!conn.ipc) {
                    PPDEBUG(ctx, conn.mod_res, "MOD PTZCMD CONNECT IPC ERROR");
                    is_err = 1;
                    break;   
                }
            }while(0);
            if(!ev_is_active(&conn.rd_io)) {
                PDEBUG("recv thread created");
                mod_ptzcmd_reciever_create(&conn, ctx);
            }
            if(conn.jrequest) {
                json_decref(conn.jrequest);
            }
            conn.mod_res[0] = '\0';
            if(is_err) {
                statemachine->stat = MOD_PTZCMD_STATUS_FAILED;
            }
            else {
                mod_ptzcmd_is_init = 1;
                conn.jrequest = json_object();
                json_object_set_new(conn.jrequest, "method", json_string("ptz_request"));
                json_object_set_new(conn.jrequest, "actions", json_array());
                json_object_set_new(conn.jrequest, "channel", json_integer(0));
                statemachine->stat = MOD_PTZCMD_STATUS_START + ctx->ipc_header.cmd;
            }
            break;
        }
        case MOD_PTZCMD_STATUS_PARAMETER_SETTING: {
            statemachine->stat = MOD_PTZCMD_STATUS_FAILED;
            int channel = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_SET_PARAM_DATA_CHANNEL), "value")));
            int address = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_SET_PARAM_DATA_ADDR), "value")));
            int speedP = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_SET_PARAM_DATA_P_SPEED), "value")));
            int speedT = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_SET_PARAM_DATA_T_SPEED), "value")));
            int speedZ = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_SET_PARAM_DATA_Z_SPEED), "value")));
            //PPDEBUG(ctx, conn.mod_res, "PTZ PARAM SETTING SUCCESS, ch: %d, add: %d, P: %d, T: %d, Z: %d",channel, address, speedP, speedT, speedZ);
            do {
                json_t *jp_actions = json_object_get(conn.jrequest, "actions");
                json_t *jp_speed = json_object();
                json_t *jp_speed_val = json_array();
                json_t *jp_addr = json_object();
                json_t *jp_channel = json_object_get(conn.jrequest, "channel");
                if(!jp_actions || !jp_speed || !jp_speed_val || !jp_addr || !jp_channel)
                    break;
                {
                    json_array_append_new(jp_speed_val, json_integer(0));
                    json_array_append_new(jp_speed_val, json_integer(speedT));
                    json_array_append_new(jp_speed_val, json_integer(speedP));
                    json_array_append_new(jp_speed_val, json_integer(speedZ));
                    json_array_append_new(jp_speed_val, json_integer(0));
                    json_object_set_new(jp_speed, "speed", jp_speed_val);
                    json_array_append_new(jp_actions, jp_speed);
                }
                {
                    json_object_set_new(jp_addr, "setaddr", json_integer(address));
                    json_array_append_new(jp_actions, jp_addr);
                }
                {
                    json_integer_set(jp_channel, channel);
                }
                if(ipc_ptz_request_send(ctx, conn.jrequest) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "send request error, cmd: %d", MOD_PTZCMD_STATUS_PARAMETER_SETTING);
                    break;
                }
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_PTZCMD_STATUS_MOVE: {
            statemachine->stat = MOD_PTZCMD_STATUS_FAILED;
            int channel = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_MOVE_DATA_CHANNEL), "value")));
            int direction = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_MOVE_DATA_DIRECTION), "value")));
            do {
                json_t *jp_actions = json_object_get(conn.jrequest, "actions");
                json_t *jp_channel = json_object_get(conn.jrequest, "channel");
                json_t *jp_direction_val = json_array();
                json_t *jp_move = json_object();
                if(!jp_actions || !jp_channel || !jp_direction_val || !jp_move)
                    break;
                if(direction > 8 || direction < 0)
                    break;
                {
                    json_array_append_new(jp_direction_val, json_string(direction_map[direction]));
                    json_array_append_new(jp_direction_val, json_integer(500));
                    json_object_set_new(jp_move, "move", jp_direction_val);
                    json_array_append_new(jp_actions, jp_move);
                }
                {
                    json_integer_set(jp_channel, channel);
                }
                if(ipc_ptz_request_send(ctx, conn.jrequest) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "send request error, cmd: %d", MOD_PTZCMD_STATUS_MOVE);
                    break;
                }
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_PTZCMD_STATUS_ZOOM: {
            statemachine->stat = MOD_PTZCMD_STATUS_FAILED;
            int channel = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_ZOOM_DATA_CHANNEL), "value")));
            int zoom = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_ZOOM_DATA_INOUT), "value")));
            do {
                json_t *jp_actions = json_object_get(conn.jrequest, "actions");
                json_t *jp_channel = json_object_get(conn.jrequest, "channel");
                json_t *jp_zoom_val = json_array();
                json_t *jp_zoom = json_object();
                if(!jp_actions || !jp_channel || !jp_zoom_val || !jp_zoom)
                    break;
                if(zoom > 1 || zoom < 0)
                    break;
                {
                    json_array_append_new(jp_zoom_val, json_string(zoom_map[zoom]));
                    json_array_append_new(jp_zoom_val, json_integer(500));
                    json_object_set_new(jp_zoom, "ptzzoom", jp_zoom_val);
                    json_array_append_new(jp_actions, jp_zoom);
                }
                {
                    json_integer_set(jp_channel, channel);
                }
                if(ipc_ptz_request_send(ctx, conn.jrequest) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "send request error, cmd: %d", MOD_PTZCMD_STATUS_ZOOM);
                    break;
                }
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_PTZCMD_STATUS_FOCUS: {
            statemachine->stat = MOD_PTZCMD_STATUS_FAILED;
            int channel = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_FOCUS_DATA_CHANNEL), "value")));
            int focus = atoi(json_string_value(json_object_get(json_array_get(ctx->data, MOD_PTZ_CMD_FOCUS_DATA_PLUS_MINUS), "value")));
            do {
                json_t *jp_actions = json_object_get(conn.jrequest, "actions");
                json_t *jp_channel = json_object_get(conn.jrequest, "channel");
                json_t *jp_focus_val = json_array();
                json_t *jp_focus = json_object();
                if(!jp_actions || !jp_channel || !jp_focus_val || !jp_focus)
                    break;
                if(focus > 1 || focus < 0)
                    break;
                {
                    json_array_append_new(jp_focus_val, json_string(focus_map[focus]));
                    json_array_append_new(jp_focus_val, json_integer(500));
                    json_object_set_new(jp_focus, "focusadj", jp_focus_val);
                    json_array_append_new(jp_actions, jp_focus);
                }
                {
                    json_integer_set(jp_channel, channel);
                }
                if(ipc_ptz_request_send(ctx, conn.jrequest) != 0) {
                    PPDEBUG(ctx, conn.mod_res, "send request error, cmd: %d", MOD_PTZCMD_STATUS_FOCUS);
                    break;
                }
                statemachine->stat = STATEMACHINE_SLEEP;
            }while(0);
            break;
        }
        case MOD_PTZCMD_STATUS_SUCCESS: {
            PPDEBUG(ctx, conn.mod_res, "test finished, success");
            if(conn.jrequest) {
                json_decref(conn.jrequest);
            }
            statemachine->stat = state_success;
            break;
        }
        case MOD_PTZCMD_STATUS_FAILED: {
            PPDEBUG(ctx, conn.mod_res, "test finished, failed");
            IPC_Destroy(conn.ipc);
            mod_ptzcmd_is_init = 0;
            if(conn.jrequest) {
                json_decref(conn.jrequest);
            }
            statemachine->stat = state_failed;
            break;
        }
        default: {
            return 0;
            break;
        }
    }
    return 1;
}
