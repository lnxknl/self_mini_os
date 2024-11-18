#ifndef _CPROTO_LIB_H
#define _CPROTO_LIB_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Protocol number (must match kernel module) */
#define IPPROTO_CPROTO 253  /* Using unassigned protocol number */

/* Maximum message size */
#define CPROTO_MAX_MSG_SIZE 1400

/* Connection timeout in seconds */
#define CPROTO_CONNECT_TIMEOUT 30

/* Error codes */
#define CPROTO_SUCCESS          0
#define CPROTO_ERR_SOCKET     -1
#define CPROTO_ERR_CONNECT    -2
#define CPROTO_ERR_SEND       -3
#define CPROTO_ERR_RECV       -4
#define CPROTO_ERR_TIMEOUT    -5

/* Library functions */
int cproto_socket(void);
int cproto_connect(int sockfd, const char *ip, int port);
int cproto_listen(int sockfd, int port);
int cproto_accept(int sockfd);
int cproto_send(int sockfd, const void *buf, size_t len);
int cproto_recv(int sockfd, void *buf, size_t len);
void cproto_close(int sockfd);

#endif /* _CPROTO_LIB_H */
