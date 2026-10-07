/*
 * IE3010 Network Programming
 * NetMessenger Client
 *
 * Registration Number: IT23569768
 * Server Port: 15768
 * NID: 5697
 *
 * File: client_9768.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 15768
#define BUFFER_SIZE 4096

int main(void)
{
    printf("NetMessenger Client\n");
    printf("Registration: IT23569768\n");
    printf("Server Port: %d\n", SERVER_PORT);

    printf("Client starter file created successfully.\n");

    return EXIT_SUCCESS;
}
