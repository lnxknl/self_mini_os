#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cproto_lib.h"

#define SERVER_PORT 8888
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sockfd;
    char buffer[BUFFER_SIZE];
    const char *message = "Hello, World!";
    int bytes_received;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    /* Create socket */
    sockfd = cproto_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    
    /* Connect to server */
    if (cproto_connect(sockfd, argv[1], SERVER_PORT) < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server %s:%d\n", argv[1], SERVER_PORT);
    
    /* Send message */
    if (cproto_send(sockfd, message, strlen(message)) < 0) {
        fprintf(stderr, "Failed to send message\n");
        cproto_close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Sent: %s\n", message);
    
    /* Receive response */
    memset(buffer, 0, BUFFER_SIZE);
    bytes_received = cproto_recv(sockfd, buffer, BUFFER_SIZE - 1);
    if (bytes_received < 0) {
        fprintf(stderr, "Failed to receive response\n");
        cproto_close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server response: %s\n", buffer);
    
    /* Close connection */
    cproto_close(sockfd);
    return 0;
}
