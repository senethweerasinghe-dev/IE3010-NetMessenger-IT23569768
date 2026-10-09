#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 15768

#define LINE_SIZE 4096
#define FILE_BUFFER 8192

static int socket_fd = -1;
static volatile int running = 1;

/* --------------------------------------------------------- */

static ssize_t send_all(
    int fd,
    const void *buffer,
    size_t length
)
{
    const unsigned char *data = buffer;
    size_t sent = 0;

    while (sent < length) {
        ssize_t result = send(
            fd,
            data + sent,
            length - sent,
            0
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (result == 0) {
            return -1;
        }

        sent += (size_t)result;
    }

    return (ssize_t)sent;
}

static ssize_t recv_line(
    int fd,
    char *buffer,
    size_t size
)
{
    size_t position = 0;

    while (position < size - 1) {
        char ch;

        ssize_t result = recv(
            fd,
            &ch,
            1,
            0
        );

        if (result == 0) {
            if (position == 0) {
                return 0;
            }

            break;
        }

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (ch == '\n') {
            break;
        }

        if (ch != '\r') {
            buffer[position++] = ch;
        }
    }

    buffer[position] = '\0';

    return (ssize_t)position;
}

static int receive_file_bytes(
    int fd,
    FILE *fp,
    long long bytes
)
{
    unsigned char buffer[FILE_BUFFER];
    long long remaining = bytes;

    while (remaining > 0) {
        size_t wanted;

        if (remaining > (long long)sizeof(buffer)) {
            wanted = sizeof(buffer);
        } else {
            wanted = (size_t)remaining;
        }

        ssize_t result = recv(
            fd,
            buffer,
            wanted,
            0
        );

        if (result <= 0) {
            return -1;
        }

        if (
            fwrite(
                buffer,
                1,
                (size_t)result,
                fp
            ) != (size_t)result
        ) {
            return -1;
        }

        remaining -= result;
    }

    return 0;
}

static void *receiver_thread(void *unused)
{
    (void)unused;

    char line[LINE_SIZE];

    while (running) {
        ssize_t result = recv_line(
            socket_fd,
            line,
            sizeof(line)
        );

        if (result <= 0) {
            if (running) {
                printf(
                    "\n[Connection closed by server]\n"
                );
            }

            running = 0;
            break;
        }

        /*
         * Internal recipient framing for file delivery:
         * MSG FILE <sender> <filename> <filesize>
         */
        if (
            strncmp(
                line,
                "MSG FILE ",
                9
            ) == 0
        ) {
            char sender[64];
            char filename[256];
            long long file_size;

            if (
                sscanf(
                    line + 9,
                    "%63s %255s %lld",
                    sender,
                    filename,
                    &file_size
                ) == 3
            ) {
                char output_name[512];

                snprintf(
                    output_name,
                    sizeof(output_name),
                    "received_%s",
                    filename
                );

                FILE *fp = fopen(
                    output_name,
                    "wb"
                );

                if (fp == NULL) {
                    fprintf(
                        stderr,
                        "\nUnable to create %s\n",
                        output_name
                    );

                    /*
                     * Still consume bytes to preserve
                     * stream framing.
                     */
                    long long remaining = file_size;
                    unsigned char discard[FILE_BUFFER];

                    while (remaining > 0) {
                        size_t wanted =
                            remaining >
                            (long long)sizeof(discard)
                                ? sizeof(discard)
                                : (size_t)remaining;

                        ssize_t n = recv(
                            socket_fd,
                            discard,
                            wanted,
                            0
                        );

                        if (n <= 0) {
                            break;
                        }

                        remaining -= n;
                    }

                    continue;
                }

                if (
                    receive_file_bytes(
                        socket_fd,
                        fp,
                        file_size
                    ) == 0
                ) {
                    fclose(fp);

                    printf(
                        "\nFILE RECEIVED from %s: %s "
                        "(%lld bytes)\n",
                        sender,
                        output_name,
                        file_size
                    );
                } else {
                    fclose(fp);

                    printf(
                        "\nFile transfer interrupted.\n"
                    );
                }

                fflush(stdout);

                continue;
            }
        }

        printf("\n%s\n", line);
        fflush(stdout);
    }

    return NULL;
}

/* ---------------------------------------------------------
   SENDFILE sender
   --------------------------------------------------------- */

static int send_file_command(const char *input)
{
    char copy[LINE_SIZE];

    char command[32];
    char target[64];
    char path[512];

    long long declared_size;

    strncpy(
        copy,
        input,
        sizeof(copy) - 1
    );

    copy[sizeof(copy) - 1] = '\0';

    if (
        sscanf(
            copy,
            "%31s %63s %511s %lld",
            command,
            target,
            path,
            &declared_size
        ) != 4
    ) {
        printf(
            "Usage: SENDFILE <target> "
            "<filename> <filesize>\n"
        );

        return -1;
    }

    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }

    long actual_size = ftell(fp);

    if (actual_size < 0) {
        perror("ftell");
        fclose(fp);
        return -1;
    }

    rewind(fp);

    if ((long long)actual_size != declared_size) {
        printf(
            "File size mismatch.\n"
            "Actual size: %ld bytes\n"
            "Command size: %lld bytes\n",
            actual_size,
            declared_size
        );

        fclose(fp);
        return -1;
    }

    /*
     * Protocol filename is sent without directory
     * components.
     */
    const char *filename = strrchr(path, '/');

    if (filename != NULL) {
        filename++;
    } else {
        filename = path;
    }

    char header[LINE_SIZE];

    snprintf(
        header,
        sizeof(header),
        "SENDFILE %s %s %lld\n",
        target,
        filename,
        declared_size
    );

    printf(
        "Sending protocol command: "
        "SENDFILE %s %s %lld\n",
        target,
        filename,
        declared_size
    );

    if (
        send_all(
            socket_fd,
            header,
            strlen(header)
        ) < 0
    ) {
        perror("send");
        fclose(fp);
        return -1;
    }

    unsigned char buffer[FILE_BUFFER];

    while (1) {
        size_t count = fread(
            buffer,
            1,
            sizeof(buffer),
            fp
        );

        if (count > 0) {
            if (
                send_all(
                    socket_fd,
                    buffer,
                    count
                ) < 0
            ) {
                perror("send file");
                fclose(fp);
                return -1;
            }
        }

        if (count < sizeof(buffer)) {
            break;
        }
    }

    fclose(fp);

    return 0;
}

/* --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    struct sockaddr_in server_address;
    const char *server_ip = "127.0.0.1";

    pthread_t receiver;

    char input[LINE_SIZE];

    signal(SIGPIPE, SIG_IGN);

    if (argc >= 2) {
        server_ip = argv[1];
    }

    socket_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (socket_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(
        &server_address,
        0,
        sizeof(server_address)
    );

    server_address.sin_family = AF_INET;
    server_address.sin_port =
        htons(SERVER_PORT);

    if (
        inet_pton(
            AF_INET,
            server_ip,
            &server_address.sin_addr
        ) <= 0
    ) {
        fprintf(
            stderr,
            "Invalid server IP: %s\n",
            server_ip
        );

        close(socket_fd);

        return EXIT_FAILURE;
    }

    printf("============================================\n");
    printf(" NetMessenger Client\n");
    printf(" Registration : IT23569768\n");
    printf(" Server       : %s:%d\n",
           server_ip,
           SERVER_PORT);
    printf("============================================\n");

    if (
        connect(
            socket_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) < 0
    ) {
        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Connected successfully.\n\n");

    printf("Available commands:\n");
    printf(" REGISTER <username>\n");
    printf(" LIST\n");
    printf(" BCAST <message>\n");
    printf(" PMSG <username> <message>\n");
    printf(" JOIN <room>\n");
    printf(" LEAVE <room>\n");
    printf(" ROOMS\n");
    printf(" RMSG <room> <message>\n");
    printf(" SENDFILE <target> <filename> <filesize>\n");
    printf(" QUIT\n\n");

    if (
        pthread_create(
            &receiver,
            NULL,
            receiver_thread,
            NULL
        ) != 0
    ) {
        perror("pthread_create");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    while (
        running &&
        fgets(
            input,
            sizeof(input),
            stdin
        ) != NULL
    ) {
        size_t length = strlen(input);

        if (
            length > 0 &&
            input[length - 1] == '\n'
        ) {
            input[length - 1] = '\0';
        }

        if (input[0] == '\0') {
            continue;
        }

        if (
            strncmp(
                input,
                "SENDFILE ",
                9
            ) == 0
        ) {
            send_file_command(input);
            continue;
        }

        {
            char outgoing[LINE_SIZE + 2];

            snprintf(
                outgoing,
                sizeof(outgoing),
                "%s\n",
                input
            );

            if (
                send_all(
                    socket_fd,
                    outgoing,
                    strlen(outgoing)
                ) < 0
            ) {
                perror("send");
                break;
            }
        }

        if (strcmp(input, "QUIT") == 0) {
            /*
             * Give receiver thread time to read
             * OK BYE before shutdown.
             */
            sleep(1);
            break;
        }
    }

    running = 0;

    shutdown(
        socket_fd,
        SHUT_RDWR
    );

    pthread_join(
        receiver,
        NULL
    );

    close(socket_fd);

    printf("Client closed.\n");

    return EXIT_SUCCESS;
}
