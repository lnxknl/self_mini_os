#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "cproto_lib.h"

int cproto_socket(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_CPROTO);
    if (sockfd < 0) {
        perror("cproto_socket");
        return CPROTO_ERR_SOCKET;
    }
    return sockfd;
}

int cproto_connect(int sockfd, const char *ip, int port) {
    struct sockaddr_in addr;
    struct timeval tv;
    
    /* Set connection timeout */
    tv.tv_sec = CPROTO_CONNECT_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        return CPROTO_ERR_CONNECT;
    }
    
    /* Prepare address structure */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        return CPROTO_ERR_CONNECT;
    }
    
    /* Connect to server */
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return CPROTO_ERR_CONNECT;
    }
    
    return CPROTO_SUCCESS;
}

int cproto_listen(int sockfd, int port) {
    struct sockaddr_in addr;
    int opt = 1;
    
    /* Set socket options */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return CPROTO_ERR_SOCKET;
    }
    
    /* Prepare address structure */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    /* Bind socket */
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return CPROTO_ERR_SOCKET;
    }
    
    /* Listen for connections */
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        return CPROTO_ERR_SOCKET;
    }
    
    return CPROTO_SUCCESS;
}

int cproto_accept(int sockfd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_socket;
    
    new_socket = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
    if (new_socket < 0) {
        perror("accept");
        return CPROTO_ERR_SOCKET;
    }
    
    printf("New connection from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
           
    return new_socket;
}

int cproto_send(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    int bytes_left = len;
    int n;
    
    while (total < len) {
        n = send(sockfd, buf + total, bytes_left, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("send");
            return CPROTO_ERR_SEND;
        }
        total += n;
        bytes_left -= n;
    }
    
    return total;
}

int cproto_recv(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    int bytes_left = len;
    int n;
    
    while (total < len) {
        n = recv(sockfd, buf + total, bytes_left, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("recv");
            return CPROTO_ERR_RECV;
        }
        if (n == 0) {
            /* Connection closed by peer */
            return total;
        }
        total += n;
        bytes_left -= n;
    }
    
    return total;
}

void cproto_close(int sockfd) {
    close(sockfd);
}
