#ifndef _MOD_IPC_
#define _MOD_IPC_
#include "ev.h"

#define IPC_SOCKET_CHECK(ipc) (ipc && IPC_Select_Object(ipc) > 0)
#define H1N1_SSCMD_MAGIC_SYNC	0x12345678

typedef struct IPC_Socket
{
    char name[128];
    int fd;
} IPC_Socket;

#define UNIX_SOCKET_LISTEN_CNT_DFL	32

IPC_Socket* IPC_Create_Server_Adv(char *path, int nListenCnt);
IPC_Socket* IPC_Create_Server(char *path);
IPC_Socket* IPC_Create_Client(char *path);
void IPC_Destroy(IPC_Socket *s);
int IPC_Send(IPC_Socket *s, unsigned char *data, size_t len);
int IPC_Recv(IPC_Socket *s, unsigned char *data, size_t len, unsigned int flag);
// get ipc socket fd
int IPC_Select_Object(IPC_Socket *s);
IPC_Socket * IPC_Server_Accept(IPC_Socket *ser, unsigned int flag);
int IPC_FullRecv(IPC_Socket *s, unsigned char *data, size_t expect_read_len, unsigned int flag);

#endif  /*  _MOD_IPC_ */
