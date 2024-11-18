#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cproto_lib.h"

#define SERVER_PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    /* Create socket */
    server_fd = cproto_socket();
    if (server_fd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    
    /* Start listening */
    if (cproto_listen(server_fd, SERVER_PORT) < 0) {
        fprintf(stderr, "Failed to listen on port %d\n", SERVER_PORT);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", SERVER_PORT);
    
    while (1) {
        /* Accept connection */
        client_fd = cproto_accept(server_fd);
        if (client_fd < 0) {
            fprintf(stderr, "Failed to accept connection\n");
            continue;
        }
        
        /* Receive data */
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = cproto_recv(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_received < 0) {
            fprintf(stderr, "Failed to receive data\n");
            cproto_close(client_fd);
            continue;
        }
        
        printf("Received: %s\n", buffer);
        
        /* Send response */
        const char *response = "Message received successfully!";
        if (cproto_send(client_fd, response, strlen(response)) < 0) {
            fprintf(stderr, "Failed to send response\n");
        }
        
        /* Close client connection */
        cproto_close(client_fd);
    }
    
    /* Close server socket */
    cproto_close(server_fd);
    return 0;
}
