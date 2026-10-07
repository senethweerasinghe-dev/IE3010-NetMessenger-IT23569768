/*
 * IE3010 Network Programming
 * NetMessenger Server
 *
 * Registration Number: IT23569768
 * Port: 15768
 * NID: 5697
 *
 * File: server_9768.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 15768
#define BUFFER_SIZE 4096
#define NID "NID:5697"

int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;

    printf("NetMessenger Server\n");
    printf("Registration: IT23569768\n");
    printf("Port: %d\n", PORT);
    printf("Node ID: %s\n", NID);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    printf("Server starter file created successfully.\n");

    close(server_socket);

    return EXIT_SUCCESS;
}
