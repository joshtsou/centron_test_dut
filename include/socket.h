#ifndef _SOCKET_H
#define _SOCKET_H
#include <sys/socket.h>
#include <arpa/inet.h> 
int socket_mcast_bind_group(char* addr_g, int mcast_port);
int socket_mcast_bind(int local_port);
int socket_mcast_sendto(int sockfd, char *mcast_group, int mcast_port, void *buf, int size);
int socket_mcast_recvfrom(int sockfd, void *buf, int len, struct sockaddr_in *remote_addr);
int socket_mcast_reply(int sockfd, int reply_port, void *buf, int len, struct sockaddr_in *remote_addr);
int socket_tcp_send(int sockfd, void *buf, int size);
int socket_tcp_recv(int sockfd, void *buf, int len);
int socket_tcp_server_bind(int port);
int socket_tcp_client_connect(char *addr_s, int port);
#endif