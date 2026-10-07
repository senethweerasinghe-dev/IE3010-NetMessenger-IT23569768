# IE3010 NetMessenger

## Student Information

Registration Number: IT23569768

## Personalised Values

- Registration Number: IT23569768
- Last Four Digits: 9768
- Server Port: 15768
- Node ID: NID:5697
- Server Source: server_9768.c
- Client Source: client_9768.c
- Makefile: Makefile_9768
- Log File: netmsg_IT23569768.log
- Storage Path: ./storage/IT23569768/<sender_username>/<filename>

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

```bash
make -f Makefile_9768
```

## Clean

```bash
make -f Makefile_9768 clean
```

## Run Server

```bash
./server_9768
```

## Run Client

```bash
./client_9768
```
