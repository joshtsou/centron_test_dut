#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "mod_ipc.h"

#ifdef DEBUG
#define YKDEBUG(fmt, ...) fprintf(stderr, "[%s:%s:%d]" fmt "\n", __FILE__, __func__, __LINE__, ## __VA_ARGS__)
#else
#define YKDEBUG(args...)
#endif

#ifndef PDEBUG
#ifdef DEBUG
#define PDEBUG(fmt, ...) do { \
        printf("[%s:%d] "fmt"\n",__FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0);
#else
#define PDEBUG(args...)  
#endif
#endif

IPC_Socket* IPC_Create_Server_Adv(char *path, int nListenCnt)
{
    IPC_Socket *s = NULL;
    int sd;
    int len;
    struct sockaddr_un local;

    if((sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        YKDEBUG("Create unix domain socket error");
        return NULL;
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    unlink(path);

    if(bind(sd, (struct sockaddr *)&local, len) == -1)
    {
        YKDEBUG("Bind unix domain socket error");
        return NULL;
    }

    if(listen(sd, (nListenCnt <= 0) ? UNIX_SOCKET_LISTEN_CNT_DFL : nListenCnt) < 0)
    {
        YKDEBUG("Listen socket error");
        close(sd);
        return NULL;
    }

    s = malloc(sizeof(IPC_Socket));
    if( ! s )
    {
        YKDEBUG("allocate memory error");
        close(sd);
        return NULL;
    }
    s->fd = sd;
    strcpy(s->name, path);
    return s;

}

IPC_Socket* IPC_Create_Server(char *path)
{
    return IPC_Create_Server_Adv(path, UNIX_SOCKET_LISTEN_CNT_DFL);
}

IPC_Socket* IPC_Create_Client(char *path)
{
    IPC_Socket *s = NULL;
    int sd, len;
    struct sockaddr_un local;

    if((sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        YKDEBUG("Create socket error");
        return NULL;
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);

    if(connect(sd, (struct sockaddr *)&local, len) < 0)
    {
        YKDEBUG("Connect '%s' socket error", path);
        close(sd);
        return NULL;
    }
    s = malloc(sizeof(IPC_Socket));
    if( ! s )
    {
        YKDEBUG("allocate memory error");
        close(sd);
        return NULL;
    }
    s->fd = sd;
    strcpy(s->name, path);
    return s;
}

void IPC_Destroy(IPC_Socket *s)
{
	if(s == NULL)
		return;
	if(s->fd > 0)
	    close(s->fd);
    free(s);
}

int IPC_Send(IPC_Socket *s, unsigned char *data, size_t len)
{
    int n;

    if((n = send(s->fd, data, len, 0)) == -1)
    {
        YKDEBUG("Send command socket error");
    }
    return n;
}

int IPC_Recv(IPC_Socket *s, unsigned char *data, size_t len, unsigned int flag)
{
    int n;

    if((n = recv(s->fd, data, len, flag)) < 0)
    {
        YKDEBUG("Receive socket error");
        return -1;
    }

    return n;
}

int IPC_FullRecv(IPC_Socket *s, unsigned char *data, size_t expect_read_len, unsigned int flag)
{
	int total_read = 0;
	while(total_read != expect_read_len)
	{
		int read_len = recv(s->fd, data + total_read, expect_read_len - total_read, flag);
		if (read_len <= 0 ) {// should not happen in blocking mode && errno != EINTR && errno != EAGAIN)
            PDEBUG("recv return %d", read_len);
			return -1;
        }
		total_read += read_len;
	}
	return total_read;
}

int IPC_Select_Object(IPC_Socket *s)
{
    return s->fd;
}

IPC_Socket * IPC_Server_Accept(IPC_Socket *ser, unsigned int flag)
{
    int rsd, len;
    struct sockaddr_un remote;
    IPC_Socket *s = NULL;

    /* len must be given initial value(sizeof(struct sockaddr_un)) for avoiding error=22(EINVAL) */
    len = sizeof(struct sockaddr_un);
    if((rsd = accept(ser->fd, (struct sockaddr *)&remote, &len)) == -1)
    {
        YKDEBUG("Accept socket error");
        return NULL;
    }

    s = malloc(sizeof(IPC_Socket));
    if( ! s )
    {
        YKDEBUG("allocate memory error");
        close(rsd);
        return NULL;
    }
    s->fd = rsd;
    strcpy(s->name, "*client*");
    return s;
}