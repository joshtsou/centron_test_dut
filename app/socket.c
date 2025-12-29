#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/nameser.h>
#include <sys/ioctl.h>
#include <resolv.h>
#include <netdb.h>
#include <ifaddrs.h>
#include "socket.h"

#ifndef PDEBUG
#define PDEBUG(fmt, ...) do { \
        printf("[%s:%d] "fmt"\n",__FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0);
#endif

int socket_tcp_send(int sockfd, void *buf, int size) {
    int ret = -1;
    do {
        if(!buf) {
            PDEBUG("socket tcp send failed: buf is nnull");
            break;
        }
        ret = send(sockfd, buf, size, 0);
        if(ret != size) {
            PDEBUG("socket tcp send error: size not correct");
            ret = -1;
            break;
        }
    }while(0);
    return ret;
}

int socket_tcp_recv(int sockfd, void *buf, int len) {
    int ret = -1;
    do {
        if(!buf) {
            PDEBUG("socket recv failed: buf is null.");
            break;
        }
        ret = recv(sockfd, buf, len, 0);
        if(ret == 0) {
            break;
        }
        if(ret != len) {
            PDEBUG("socket recv failed, recv size: %d, should be: %d", ret, len);
            ret = -1;
            break;
        }
    }while(0);
    return ret;
}

int socket_tcp_server_bind(int port) {
    int sockfd = -1;
    struct sockaddr_in server_addr;
    int opt = 1;
    do {
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation failed");
            PDEBUG("tcp server bind failed: socket error");
            break;
        }
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Bind failed");
            PDEBUG("tcp server bind failed: bind failed");
            close(sockfd);
            break;
        }
        if (listen(sockfd, 5) < 0) {
            perror("Listen failed");
            PDEBUG("tcp server bind failed: listen error");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        // struct sockaddr_in local_addr;
        // socklen_t len = sizeof(local_addr);

        // if (getsockname(sockfd, (struct sockaddr *)&local_addr, &len) == -1) {
        //     perror("getsockname failed");
        // } else {
        //     char ip_str[INET_ADDRSTRLEN];
            
        //     inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str));
        //     int actual_port = ntohs(local_addr.sin_port);

        //     printf("--------------------------------------\n");
        //     printf("Bound IP   : %s\n", ip_str);
        //     printf("Bound Port : %d\n", actual_port);
        //     printf("--------------------------------------\n");
        // }
    }while(0);
    return sockfd; 
}

int socket_tcp_client_connect(char *addr_s, int port) {
    int sockfd = -1;
    struct sockaddr_in addr;
    do {
        if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            PDEBUG("tcp client socket failed");
            break;
        }
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, addr_s, &addr.sin_addr);
        PDEBUG("try to cennect DUT, IP: %s, port: %d",addr_s, port);
        if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            PDEBUG("tcp connect error");
            close(sockfd);
            sockfd = -1;
            break;
        }
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    }while(0);
    return sockfd;
}

int socket_mcast_bind_group(char* addr_g, int mcast_port) {
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int sockfd;
    int reuse = 1;
    do {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd == -1) {
            PDEBUG("socket failed");
            break;
        }    
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(mcast_port);
        bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));

        mreq.imr_multiaddr.s_addr = inet_addr(addr_g);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    } while(0);
    return sockfd;
}

int socket_mcast_bind(int local_port) {
    int sockfd;
    struct sockaddr_in local_addr;
    int size = 8 * 1024 * 1024;
    int reuse = 1;
    do {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd == -1) {
            PDEBUG("socket failed");
            break;
        }
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(local_port);
        bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr));
    } while(0);
    return sockfd;
}

int socket_mcast_sendto(int sockfd, char *mcast_group, int mcast_port, void *buf, int size) {
    struct sockaddr_in mcast_addr;
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_addr.s_addr = inet_addr(mcast_group);
    mcast_addr.sin_port = htons(mcast_port);
    int ret = -1;
    do {
        if(!buf) {
            PDEBUG("socket send failed: buf is null");
            break;
        }
        if ((ret = sendto(sockfd, buf, size, 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr))) < 0) {
            PDEBUG("socket send failed: sendto error");
            break;
        }
        if(ret != size) {
            PDEBUG("socket send error: size not match. send size: %d, should be %d\n", ret, size);
            ret = -1;
            break;
        }
    }while(0);
    return ret;
}

int socket_mcast_recvfrom(int sockfd, void *buf, int len, struct sockaddr_in *remote_addr) {
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int ret = -1;
    do {
        if(!remote_addr) {
            PDEBUG("socket recv failed: addr is null.")
            break;
        }
        if(!buf) {
            PDEBUG("socket recv failed: buf is null.");
            break;
        }
        ret = recvfrom(sockfd, buf, len, 0, (struct sockaddr*)remote_addr, &addrlen);
        if(ret != len) {
            PDEBUG("socket recv failed, recv size: %d, should be: %d", ret, len);
            ret = -1;
            break;
        }
    }while(0);
    return ret;
}

int socket_mcast_reply(int sockfd, int reply_port, void *buf, int len, struct sockaddr_in *remote_addr) {
    int ret = -1;
    struct sockaddr_in reply_addr;
    do {
        if(!remote_addr) {
            PDEBUG("socket reply failed: addr is null.")
            break;
        }
        if(!buf) {
            PDEBUG("socket reply failed: buf is null.");
            break;
        }
        memset(&reply_addr, 0, sizeof(reply_addr));
        reply_addr.sin_family = AF_INET;
        reply_addr.sin_port = htons(reply_port);
        // reply_addr.sin_addr = remote_addr->sin_addr;
        reply_addr.sin_addr.s_addr = remote_addr->sin_addr.s_addr;
        if ((ret = sendto(sockfd, buf, len, 0, (struct sockaddr*)&reply_addr, sizeof(reply_addr))) != len) {
            PDEBUG("scoket reply failed. send size: %d, should be: %d", ret, len);
            ret = -1;
            break;
        }
    }while(0);
    return ret;
}
