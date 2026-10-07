#!/bin/bash

set -e

REGNO="IT23569768"
LAST4="9768"
PORT="15768"
NID="5697"

SERVER_FILE="server_${LAST4}.c"
CLIENT_FILE="client_${LAST4}.c"
MAKEFILE="Makefile_${LAST4}"
LOG_FILE="netmsg_${REGNO}.log"

echo "=========================================="
echo " IE3010 NetMessenger Project Setup"
echo " Registration Number: $REGNO"
echo " Port: $PORT"
echo " NID: $NID"
echo "=========================================="

# --------------------------------------------------
# Create directories
# --------------------------------------------------

mkdir -p storage/$REGNO
mkdir -p test_files
mkdir -p docs

echo "Directories created."

# --------------------------------------------------
# Create server source file
# --------------------------------------------------

cat > "$SERVER_FILE" <<EOF
/*
 * IE3010 Network Programming
 * NetMessenger Server
 *
 * Registration Number: $REGNO
 * Port: $PORT
 * NID: $NID
 *
 * File: $SERVER_FILE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT $PORT
#define BUFFER_SIZE 4096
#define NID "NID:$NID"

int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;

    printf("NetMessenger Server\\n");
    printf("Registration: $REGNO\\n");
    printf("Port: %d\\n", PORT);
    printf("Node ID: %s\\n", NID);

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

    printf("Server starter file created successfully.\\n");

    close(server_socket);

    return EXIT_SUCCESS;
}
EOF

echo "$SERVER_FILE created."

# --------------------------------------------------
# Create client source file
# --------------------------------------------------

cat > "$CLIENT_FILE" <<EOF
/*
 * IE3010 Network Programming
 * NetMessenger Client
 *
 * Registration Number: $REGNO
 * Server Port: $PORT
 * NID: $NID
 *
 * File: $CLIENT_FILE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT $PORT
#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
    printf("NetMessenger Client\\n");
    printf("Registration: $REGNO\\n");
    printf("Server Port: %d\\n", SERVER_PORT);

    printf("Client starter file created successfully.\\n");

    return EXIT_SUCCESS;
}
EOF

echo "$CLIENT_FILE created."

# --------------------------------------------------
# Create Makefile
# --------------------------------------------------

cat > "$MAKEFILE" <<EOF
CC = gcc
CFLAGS = -Wall -Wextra -pthread

SERVER = server_$LAST4
CLIENT = client_$LAST4

SERVER_SRC = $SERVER_FILE
CLIENT_SRC = $CLIENT_FILE

all: \$(SERVER) \$(CLIENT)

\$(SERVER): \$(SERVER_SRC)
	\$(CC) \$(CFLAGS) \$(SERVER_SRC) -o \$(SERVER)

\$(CLIENT): \$(CLIENT_SRC)
	\$(CC) \$(CFLAGS) \$(CLIENT_SRC) -o \$(CLIENT)

clean:
	rm -f \$(SERVER) \$(CLIENT)

.PHONY: all clean
EOF

echo "$MAKEFILE created."

# --------------------------------------------------
# README
# --------------------------------------------------

cat > README.md <<EOF
# IE3010 NetMessenger

## Student Information

Registration Number: $REGNO

## Personalised Values

- Registration Number: $REGNO
- Last Four Digits: $LAST4
- Server Port: $PORT
- Node ID: NID:$NID
- Server Source: $SERVER_FILE
- Client Source: $CLIENT_FILE
- Makefile: $MAKEFILE
- Log File: $LOG_FILE
- Storage Path: ./storage/$REGNO/<sender_username>/<filename>

## Assignment

NetMessenger is a multi-client TCP/IP chat and file-sharing system implemented in C using BSD sockets.

## Required Features

- Multiple simultaneous clients
- Unique username registration
- User presence
- Broadcast messaging
- Private messaging
- Chat rooms
- Room messaging
- File sharing
- Graceful disconnect
- Error handling
- Server-side logging

## Build

Run:

\`\`\`bash
make -f $MAKEFILE
\`\`\`

## Clean

\`\`\`bash
make -f $MAKEFILE clean
\`\`\`

## Run Server

\`\`\`bash
./server_$LAST4
\`\`\`

## Run Client

\`\`\`bash
./client_$LAST4
\`\`\`
EOF

echo "README.md created."

# --------------------------------------------------
# Design Diary
# --------------------------------------------------

cat > DESIGN_DIARY.md <<EOF
# Design Diary

## Student

Registration Number: $REGNO

## Entry 1 - Initial Setup

Created the NetMessenger project structure and personalised the project using my registration number.

Personalised values:

- Port: $PORT
- NID: $NID
- Server file: $SERVER_FILE
- Client file: $CLIENT_FILE
- Makefile: $MAKEFILE
- Log file: $LOG_FILE

Initial design decision:

The project will use TCP sockets.

The concurrency model will be decided and justified during implementation.

## Challenges

Add real problems encountered during development here.

## Decisions

Add important design decisions here as the project develops.
EOF

echo "DESIGN_DIARY.md created."

# --------------------------------------------------
# Prompt Log
# --------------------------------------------------

cat > PROMPT_LOG.md <<EOF
# AI Prompt Log

## Student

Registration Number: $REGNO

---

## Entry 1

Date:

AI Tool:
ChatGPT

Prompt:

Asked for guidance on setting up the IE3010 NetMessenger project structure on CentOS.

How the output was used:

Used the guidance to create the personalised project structure and starter files.

Changes / Evaluation:

I reviewed the generated structure and verified the personalised values against the assignment specification.

---

Add each substantive AI interaction below this entry.
EOF

echo "PROMPT_LOG.md created."

# --------------------------------------------------
# Testing document
# --------------------------------------------------

cat > TESTING.md <<EOF
# NetMessenger Testing Record

Registration Number: $REGNO

| Test ID | Feature | Test | Expected Result | Actual Result | Status |
|---------|---------|------|-----------------|---------------|--------|
| T01 | Server Startup | Start server | Server listens on port $PORT | | |
| T02 | Registration | Register new username | Registration succeeds | | |
| T03 | Duplicate Username | Register same username twice | USERNAME_TAKEN error | | |
| T04 | User List | Send LIST | Connected users returned | | |
| T05 | Broadcast | Send BCAST message | Other clients receive message | | |
| T06 | Private Message | Send PMSG | Target user receives message | | |
| T07 | Join Room | Send JOIN | User joins or creates room | | |
| T08 | Leave Room | Send LEAVE | User leaves room | | |
| T09 | List Rooms | Send ROOMS | Available rooms returned | | |
| T10 | Room Message | Send RMSG | Room members receive message | | |
| T11 | File Transfer | Send SENDFILE | Complete file transferred | | |
| T12 | Invalid User | Message unknown user | Error returned | | |
| T13 | Disconnect | Kill client unexpectedly | Server remains running | | |
EOF

echo "TESTING.md created."

# --------------------------------------------------
# Reflection template
# --------------------------------------------------

cat > REFLECTION.md <<EOF
# Reflection

Registration Number: $REGNO

Target length: 300-500 words.

## 1. Which AI tools did I use and at which stages?

Write your own answer.

## 2. What did AI do well and where was it wrong or misleading?

Write your own answer.

## 3. What AI output did I change, add, or reject, and why?

Write your own answer.

## 4. What did I learn about network programming?

Write your own answer.
EOF

echo "REFLECTION.md created."

# --------------------------------------------------
# Gitignore
# --------------------------------------------------

cat > .gitignore <<EOF
server_$LAST4
client_$LAST4
*.o
*.tmp
*.swp
core
core.*
.DS_Store
EOF

echo ".gitignore created."

# --------------------------------------------------
# Test file
# --------------------------------------------------

echo "NetMessenger test file - $REGNO" > test_files/sample.txt

# --------------------------------------------------
# Empty personalised log
# --------------------------------------------------

touch "$LOG_FILE"

echo "$LOG_FILE created."

echo ""
echo "=========================================="
echo "Project setup completed successfully."
echo "=========================================="
echo ""
echo "Files:"
echo ""

ls -la

echo ""
echo "Next command:"
echo ""
echo "make -f $MAKEFILE"
echo ""
