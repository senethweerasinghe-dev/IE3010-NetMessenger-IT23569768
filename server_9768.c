#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 15768
#define NID "NID:5697"

#define MAX_CLIENTS 64
#define MAX_ROOMS 32
#define MAX_ROOM_MEMBERS 64

#define USERNAME_LEN 64
#define ROOM_LEN 64
#define LINE_SIZE 4096
#define FILE_BUFFER 8192

/* Assignment does not provide a numeric maximum. */
#define MAX_FILE_SIZE (10LL * 1024LL * 1024LL)

#define LOG_FILE "netmsg_IT23569768.log"
#define STORAGE_ROOT "./storage/IT23569768"

typedef struct {
    int fd;
    int active;
    int registered;
    char username[USERNAME_LEN];
} Client;

typedef struct {
    int active;
    char name[ROOM_LEN];

    char members[MAX_ROOM_MEMBERS][USERNAME_LEN];
    int member_count;
} Room;

static Client clients[MAX_CLIENTS];
static Room rooms[MAX_ROOMS];

static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------------------------------------------
   Utility
   --------------------------------------------------------- */

static void log_event(const char *format, ...)
{
    FILE *fp;
    time_t now;
    struct tm tm_now;
    char timestamp[64];

    pthread_mutex_lock(&log_mutex);

    fp = fopen(LOG_FILE, "a");

    if (fp != NULL) {
        now = time(NULL);
        localtime_r(&now, &tm_now);

        strftime(
            timestamp,
            sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S",
            &tm_now
        );

        fprintf(fp, "[%s] ", timestamp);

        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);

        fputc('\n', fp);
        fclose(fp);
    }

    pthread_mutex_unlock(&log_mutex);
}

static ssize_t send_all_raw(int fd, const void *buffer, size_t length)
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

static int send_text_locked(int fd, const char *text)
{
    return send_all_raw(fd, text, strlen(text)) < 0 ? -1 : 0;
}

static int send_text(int fd, const char *text)
{
    int result;

    pthread_mutex_lock(&send_mutex);
    result = send_text_locked(fd, text);
    pthread_mutex_unlock(&send_mutex);

    return result;
}

static void send_response(int fd, const char *response)
{
    char buffer[LINE_SIZE];

    snprintf(
        buffer,
        sizeof(buffer),
        "%s %s\n",
        response,
        NID
    );

    send_text(fd, buffer);
}

static ssize_t recv_line(int fd, char *buffer, size_t size)
{
    size_t position = 0;

    if (size == 0) {
        return -1;
    }

    while (position < size - 1) {
        char ch;

        ssize_t result = recv(fd, &ch, 1, 0);

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

static int receive_exact(
    int fd,
    FILE *output,
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

        if (result == 0) {
            return -1;
        }

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (output != NULL) {
            if (fwrite(
                    buffer,
                    1,
                    (size_t)result,
                    output
                ) != (size_t)result) {
                return -1;
            }
        }

        remaining -= result;
    }

    return 0;
}

static const char *safe_filename(const char *filename)
{
    const char *slash = strrchr(filename, '/');

    if (slash != NULL) {
        return slash + 1;
    }

    return filename;
}

static void create_storage_directories(
    const char *username
)
{
    char path[512];

    mkdir("./storage", 0755);
    mkdir(STORAGE_ROOT, 0755);

    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        STORAGE_ROOT,
        username
    );

    mkdir(path, 0755);
}

/* ---------------------------------------------------------
   Client / room lookup
   --------------------------------------------------------- */

static int find_user_index_locked(const char *username)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (
            clients[i].active &&
            clients[i].registered &&
            strcmp(clients[i].username, username) == 0
        ) {
            return i;
        }
    }

    return -1;
}

static int find_room_index_locked(const char *name)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (
            rooms[i].active &&
            strcmp(rooms[i].name, name) == 0
        ) {
            return i;
        }
    }

    return -1;
}

static int create_room_locked(const char *name)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            rooms[i].active = 1;

            strncpy(
                rooms[i].name,
                name,
                ROOM_LEN - 1
            );

            rooms[i].name[ROOM_LEN - 1] = '\0';
            rooms[i].member_count = 0;

            return i;
        }
    }

    return -1;
}

static int room_has_member_locked(
    Room *room,
    const char *username
)
{
    int i;

    for (i = 0; i < room->member_count; i++) {
        if (
            strcmp(
                room->members[i],
                username
            ) == 0
        ) {
            return 1;
        }
    }

    return 0;
}

static int add_room_member_locked(
    Room *room,
    const char *username
)
{
    if (room_has_member_locked(room, username)) {
        return 0;
    }

    if (room->member_count >= MAX_ROOM_MEMBERS) {
        return -1;
    }

    strncpy(
        room->members[room->member_count],
        username,
        USERNAME_LEN - 1
    );

    room->members[room->member_count]
                 [USERNAME_LEN - 1] = '\0';

    room->member_count++;

    return 0;
}

static void remove_room_member_locked(
    Room *room,
    const char *username
)
{
    int i;

    for (i = 0; i < room->member_count; i++) {
        if (
            strcmp(
                room->members[i],
                username
            ) == 0
        ) {
            int j;

            for (
                j = i;
                j < room->member_count - 1;
                j++
            ) {
                strcpy(
                    room->members[j],
                    room->members[j + 1]
                );
            }

            room->member_count--;

            break;
        }
    }

    if (room->member_count == 0) {
        room->active = 0;
        room->name[0] = '\0';
    }
}

static void remove_user_from_all_rooms_locked(
    const char *username
)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            remove_room_member_locked(
                &rooms[i],
                username
            );
        }
    }
}

/* ---------------------------------------------------------
   Broadcast / presence
   --------------------------------------------------------- */

static void broadcast_except(
    int excluded_fd,
    const char *message
)
{
    int fds[MAX_CLIENTS];
    int count = 0;
    int i;

    pthread_mutex_lock(&state_mutex);

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (
            clients[i].active &&
            clients[i].registered &&
            clients[i].fd != excluded_fd
        ) {
            fds[count++] = clients[i].fd;
        }
    }

    pthread_mutex_unlock(&state_mutex);

    for (i = 0; i < count; i++) {
        send_text(fds[i], message);
    }
}

static void notify_join(
    int fd,
    const char *username
)
{
    char message[LINE_SIZE];

    snprintf(
        message,
        sizeof(message),
        "MSG BCAST SERVER %s joined\n",
        username
    );

    broadcast_except(fd, message);
}

static void notify_leave(const char *username)
{
    char message[LINE_SIZE];

    snprintf(
        message,
        sizeof(message),
        "MSG BCAST SERVER %s left\n",
        username
    );

    broadcast_except(-1, message);
}

/* ---------------------------------------------------------
   File delivery
   --------------------------------------------------------- */

static int send_stored_file(
    int fd,
    const char *sender,
    const char *filename,
    long long file_size,
    const char *storage_path
)
{
    FILE *fp;
    unsigned char buffer[FILE_BUFFER];
    char header[LINE_SIZE];

    fp = fopen(storage_path, "rb");

    if (fp == NULL) {
        return -1;
    }

    snprintf(
        header,
        sizeof(header),
        "MSG FILE %s %s %lld\n",
        sender,
        filename,
        file_size
    );

    pthread_mutex_lock(&send_mutex);

    if (send_text_locked(fd, header) < 0) {
        pthread_mutex_unlock(&send_mutex);
        fclose(fp);
        return -1;
    }

    while (1) {
        size_t count = fread(
            buffer,
            1,
            sizeof(buffer),
            fp
        );

        if (count > 0) {
            if (
                send_all_raw(
                    fd,
                    buffer,
                    count
                ) < 0
            ) {
                pthread_mutex_unlock(&send_mutex);
                fclose(fp);
                return -1;
            }
        }

        if (count < sizeof(buffer)) {
            break;
        }
    }

    pthread_mutex_unlock(&send_mutex);

    fclose(fp);

    return 0;
}

/* ---------------------------------------------------------
   Individual protocol handlers
   --------------------------------------------------------- */

static void handle_register(
    int client_index,
    const char *username
)
{
    int fd = clients[client_index].fd;

    if (
        username == NULL ||
        username[0] == '\0' ||
        strchr(username, ' ') != NULL
    ) {
        send_response(
            fd,
            "ERR 005 INVALID_USERNAME"
        );

        return;
    }

    pthread_mutex_lock(&state_mutex);

    if (clients[client_index].registered) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 006 ALREADY_REGISTERED"
        );

        return;
    }

    if (find_user_index_locked(username) >= 0) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 001 USERNAME_TAKEN"
        );

        return;
    }

    strncpy(
        clients[client_index].username,
        username,
        USERNAME_LEN - 1
    );

    clients[client_index]
           .username[USERNAME_LEN - 1] = '\0';

    clients[client_index].registered = 1;

    pthread_mutex_unlock(&state_mutex);

    {
        char response[LINE_SIZE];

        snprintf(
            response,
            sizeof(response),
            "OK REGISTERED %s",
            username
        );

        send_response(fd, response);
    }

    log_event(
        "REGISTER username=%s",
        username
    );

    notify_join(fd, username);
}

static void handle_list(int fd)
{
    char response[LINE_SIZE];
    size_t used = 0;
    int first = 1;
    int i;

    used += (size_t)snprintf(
        response + used,
        sizeof(response) - used,
        "OK USERS "
    );

    pthread_mutex_lock(&state_mutex);

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (
            clients[i].active &&
            clients[i].registered
        ) {
            if (!first) {
                used += (size_t)snprintf(
                    response + used,
                    sizeof(response) - used,
                    ","
                );
            }

            used += (size_t)snprintf(
                response + used,
                sizeof(response) - used,
                "%s",
                clients[i].username
            );

            first = 0;

            if (used >= sizeof(response) - 100) {
                break;
            }
        }
    }

    pthread_mutex_unlock(&state_mutex);

    send_response(fd, response);
}

static void handle_broadcast(
    int fd,
    const char *sender,
    const char *message
)
{
    char outgoing[LINE_SIZE];

    if (message == NULL || message[0] == '\0') {
        send_response(
            fd,
            "ERR 005 INVALID_COMMAND"
        );

        return;
    }

    snprintf(
        outgoing,
        sizeof(outgoing),
        "MSG BCAST %s %s\n",
        sender,
        message
    );

    broadcast_except(fd, outgoing);

    send_response(fd, "OK SENT");

    log_event(
        "BCAST sender=%s message=%s",
        sender,
        message
    );
}

static void handle_private(
    int fd,
    const char *sender,
    char *arguments
)
{
    char *save = NULL;
    char *target;
    char *message;
    int target_fd = -1;
    int target_index;

    target = strtok_r(arguments, " ", &save);
    message = strtok_r(NULL, "", &save);

    if (
        target == NULL ||
        message == NULL ||
        message[0] == '\0'
    ) {
        send_response(
            fd,
            "ERR 005 INVALID_COMMAND"
        );

        return;
    }

    pthread_mutex_lock(&state_mutex);

    target_index = find_user_index_locked(target);

    if (target_index >= 0) {
        target_fd = clients[target_index].fd;
    }

    pthread_mutex_unlock(&state_mutex);

    if (target_fd < 0) {
        send_response(
            fd,
            "ERR 002 USER_NOT_FOUND"
        );

        return;
    }

    {
        char outgoing[LINE_SIZE];

        snprintf(
            outgoing,
            sizeof(outgoing),
            "MSG PRIV %s %s\n",
            sender,
            message
        );

        send_text(target_fd, outgoing);
    }

    send_response(fd, "OK SENT");

    log_event(
        "PMSG sender=%s target=%s message=%s",
        sender,
        target,
        message
    );
}

static void handle_join(
    int fd,
    const char *username,
    const char *room_name
)
{
    int index;

    if (
        room_name == NULL ||
        room_name[0] == '\0' ||
        strchr(room_name, ' ') != NULL
    ) {
        send_response(
            fd,
            "ERR 005 INVALID_ROOM"
        );

        return;
    }

    pthread_mutex_lock(&state_mutex);

    index = find_room_index_locked(room_name);

    if (index < 0) {
        index = create_room_locked(room_name);
    }

    if (index < 0) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 007 ROOM_LIMIT_REACHED"
        );

        return;
    }

    if (
        add_room_member_locked(
            &rooms[index],
            username
        ) < 0
    ) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 008 ROOM_FULL"
        );

        return;
    }

    pthread_mutex_unlock(&state_mutex);

    {
        char response[LINE_SIZE];

        snprintf(
            response,
            sizeof(response),
            "OK JOINED %s",
            room_name
        );

        send_response(fd, response);
    }

    log_event(
        "JOIN username=%s room=%s",
        username,
        room_name
    );
}

static void handle_leave_room(
    int fd,
    const char *username,
    const char *room_name
)
{
    int index;

    pthread_mutex_lock(&state_mutex);

    index = find_room_index_locked(room_name);

    if (index < 0) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 003 ROOM_NOT_FOUND"
        );

        return;
    }

    remove_room_member_locked(
        &rooms[index],
        username
    );

    pthread_mutex_unlock(&state_mutex);

    {
        char response[LINE_SIZE];

        snprintf(
            response,
            sizeof(response),
            "OK LEFT %s",
            room_name
        );

        send_response(fd, response);
    }

    log_event(
        "LEAVE username=%s room=%s",
        username,
        room_name
    );
}

static void handle_rooms(int fd)
{
    char response[LINE_SIZE];
    size_t used = 0;
    int first = 1;
    int i;

    used += (size_t)snprintf(
        response + used,
        sizeof(response) - used,
        "OK ROOMS "
    );

    pthread_mutex_lock(&state_mutex);

    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            if (!first) {
                used += (size_t)snprintf(
                    response + used,
                    sizeof(response) - used,
                    ","
                );
            }

            used += (size_t)snprintf(
                response + used,
                sizeof(response) - used,
                "%s",
                rooms[i].name
            );

            first = 0;
        }
    }

    pthread_mutex_unlock(&state_mutex);

    send_response(fd, response);
}

static void handle_room_message(
    int fd,
    const char *sender,
    char *arguments
)
{
    char *save = NULL;
    char *room_name;
    char *message;

    int room_index;
    int target_fds[MAX_ROOM_MEMBERS];
    int target_count = 0;
    int sender_is_member = 0;

    int i;

    room_name = strtok_r(arguments, " ", &save);
    message = strtok_r(NULL, "", &save);

    if (
        room_name == NULL ||
        message == NULL ||
        message[0] == '\0'
    ) {
        send_response(
            fd,
            "ERR 005 INVALID_COMMAND"
        );

        return;
    }

    pthread_mutex_lock(&state_mutex);

    room_index = find_room_index_locked(room_name);

    if (room_index < 0) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 003 ROOM_NOT_FOUND"
        );

        return;
    }

    sender_is_member = room_has_member_locked(
        &rooms[room_index],
        sender
    );

    if (!sender_is_member) {
        pthread_mutex_unlock(&state_mutex);

        send_response(
            fd,
            "ERR 009 NOT_IN_ROOM"
        );

        return;
    }

    for (
        i = 0;
        i < rooms[room_index].member_count;
        i++
    ) {
        int client_index = find_user_index_locked(
            rooms[room_index].members[i]
        );

        if (client_index >= 0) {
            target_fds[target_count++] =
                clients[client_index].fd;
        }
    }

    pthread_mutex_unlock(&state_mutex);

    {
        char outgoing[LINE_SIZE];

        snprintf(
            outgoing,
            sizeof(outgoing),
            "MSG ROOM %s %s %s\n",
            room_name,
            sender,
            message
        );

        for (i = 0; i < target_count; i++) {
            send_text(
                target_fds[i],
                outgoing
            );
        }
    }

    send_response(fd, "OK SENT");

    log_event(
        "RMSG sender=%s room=%s message=%s",
        sender,
        room_name,
        message
    );
}

static void handle_sendfile(
    int fd,
    const char *sender,
    char *arguments
)
{
    char target[USERNAME_LEN];
    char filename_input[256];
    long long file_size;

    const char *filename;
    char storage_path[768];

    int user_target_index = -1;
    int room_target_index = -1;

    int recipient_fds[MAX_CLIENTS];
    int recipient_count = 0;

    int i;

    if (
        sscanf(
            arguments,
            "%63s %255s %lld",
            target,
            filename_input,
            &file_size
        ) != 3 ||
        file_size < 0
    ) {
        send_response(
            fd,
            "ERR 005 INVALID_COMMAND"
        );

        return;
    }

    if (file_size > MAX_FILE_SIZE) {
        /*
         * Raw bytes immediately follow the command,
         * so they still have to be consumed.
         */
        receive_exact(
            fd,
            NULL,
            file_size
        );

        send_response(
            fd,
            "ERR 004 FILE_TOO_LARGE"
        );

        return;
    }

    pthread_mutex_lock(&state_mutex);

    user_target_index =
        find_user_index_locked(target);

    if (user_target_index < 0) {
        room_target_index =
            find_room_index_locked(target);
    }

    if (user_target_index >= 0) {
        recipient_fds[recipient_count++] =
            clients[user_target_index].fd;
    } else if (room_target_index >= 0) {
        Room *room = &rooms[room_target_index];

        for (
            i = 0;
            i < room->member_count;
            i++
        ) {
            if (
                strcmp(
                    room->members[i],
                    sender
                ) == 0
            ) {
                continue;
            }

            {
                int ci = find_user_index_locked(
                    room->members[i]
                );

                if (ci >= 0) {
                    recipient_fds[recipient_count++] =
                        clients[ci].fd;
                }
            }
        }
    }

    pthread_mutex_unlock(&state_mutex);

    if (
        user_target_index < 0 &&
        room_target_index < 0
    ) {
        /* Consume the immediately-following raw data. */
        receive_exact(
            fd,
            NULL,
            file_size
        );

        send_response(
            fd,
            "ERR 002 USER_OR_ROOM_NOT_FOUND"
        );

        return;
    }

    filename = safe_filename(filename_input);

    create_storage_directories(sender);

    snprintf(
        storage_path,
        sizeof(storage_path),
        "%s/%s/%s",
        STORAGE_ROOT,
        sender,
        filename
    );

    {
        FILE *fp = fopen(storage_path, "wb");

        if (fp == NULL) {
            receive_exact(
                fd,
                NULL,
                file_size
            );

            send_response(
                fd,
                "ERR 010 FILE_STORAGE_ERROR"
            );

            return;
        }

        if (
            receive_exact(
                fd,
                fp,
                file_size
            ) < 0
        ) {
            fclose(fp);

            send_response(
                fd,
                "ERR 011 FILE_TRANSFER_FAILED"
            );

            return;
        }

        fclose(fp);
    }

    for (i = 0; i < recipient_count; i++) {
        send_stored_file(
            recipient_fds[i],
            sender,
            filename,
            file_size,
            storage_path
        );
    }

    {
        char response[LINE_SIZE];

        snprintf(
            response,
            sizeof(response),
            "OK FILE_RECEIVED %s",
            filename
        );

        send_response(fd, response);
    }

    log_event(
        "SENDFILE sender=%s target=%s file=%s bytes=%lld",
        sender,
        target,
        filename,
        file_size
    );
}

/* ---------------------------------------------------------
   Client thread
   --------------------------------------------------------- */

static void disconnect_client(int client_index)
{
    int fd;
    int was_registered;
    char username[USERNAME_LEN];

    pthread_mutex_lock(&state_mutex);

    fd = clients[client_index].fd;
    was_registered =
        clients[client_index].registered;

    strncpy(
        username,
        clients[client_index].username,
        sizeof(username) - 1
    );

    username[sizeof(username) - 1] = '\0';

    if (was_registered) {
        remove_user_from_all_rooms_locked(
            username
        );
    }

    clients[client_index].active = 0;
    clients[client_index].registered = 0;
    clients[client_index].username[0] = '\0';
    clients[client_index].fd = -1;

    pthread_mutex_unlock(&state_mutex);

    if (fd >= 0) {
        close(fd);
    }

    if (was_registered) {
        log_event(
            "DISCONNECT username=%s",
            username
        );

        notify_leave(username);
    } else {
        log_event(
            "DISCONNECT unregistered client"
        );
    }
}

static void *client_thread(void *argument)
{
    int client_index = *(int *)argument;
    int fd;

    char line[LINE_SIZE];

    free(argument);

    pthread_mutex_lock(&state_mutex);
    fd = clients[client_index].fd;
    pthread_mutex_unlock(&state_mutex);

    while (1) {
        ssize_t length = recv_line(
            fd,
            line,
            sizeof(line)
        );

        if (length <= 0) {
            break;
        }

        if (line[0] == '\0') {
            send_response(
                fd,
                "ERR 005 INVALID_COMMAND"
            );

            continue;
        }

        if (strncmp(line, "REGISTER ", 9) == 0) {
            handle_register(
                client_index,
                line + 9
            );

            continue;
        }

        pthread_mutex_lock(&state_mutex);

        int registered =
            clients[client_index].registered;

        char username[USERNAME_LEN];

        strncpy(
            username,
            clients[client_index].username,
            sizeof(username) - 1
        );

        username[sizeof(username) - 1] = '\0';

        pthread_mutex_unlock(&state_mutex);

        if (!registered) {
            send_response(
                fd,
                "ERR 012 REGISTER_REQUIRED"
            );

            continue;
        }

        if (strcmp(line, "LIST") == 0) {
            handle_list(fd);
        }
        else if (strncmp(line, "BCAST ", 6) == 0) {
            handle_broadcast(
                fd,
                username,
                line + 6
            );
        }
        else if (strncmp(line, "PMSG ", 5) == 0) {
            handle_private(
                fd,
                username,
                line + 5
            );
        }
        else if (strncmp(line, "JOIN ", 5) == 0) {
            handle_join(
                fd,
                username,
                line + 5
            );
        }
        else if (strncmp(line, "LEAVE ", 6) == 0) {
            handle_leave_room(
                fd,
                username,
                line + 6
            );
        }
        else if (strcmp(line, "ROOMS") == 0) {
            handle_rooms(fd);
        }
        else if (strncmp(line, "RMSG ", 5) == 0) {
            handle_room_message(
                fd,
                username,
                line + 5
            );
        }
        else if (strncmp(line, "SENDFILE ", 9) == 0) {
            handle_sendfile(
                fd,
                username,
                line + 9
            );
        }
        else if (strcmp(line, "QUIT") == 0) {
            send_response(
                fd,
                "OK BYE"
            );

            break;
        }
        else {
            send_response(
                fd,
                "ERR 005 INVALID_COMMAND"
            );
        }
    }

    disconnect_client(client_index);

    return NULL;
}

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void)
{
    int server_fd;
    int option = 1;

    struct sockaddr_in address;

    int i;

    signal(SIGPIPE, SIG_IGN);

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = 0;
        clients[i].registered = 0;
    }

    memset(rooms, 0, sizeof(rooms));

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (
        setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)
        ) < 0
    ) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (
        bind(
            server_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0
    ) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (
        listen(
            server_fd,
            MAX_CLIENTS
        ) < 0
    ) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("============================================\n");
    printf(" NetMessenger Server\n");
    printf(" Registration : IT23569768\n");
    printf(" Port         : %d\n", PORT);
    printf(" Node ID      : %s\n", NID);
    printf(" Log          : %s\n", LOG_FILE);
    printf(" Storage      : %s\n", STORAGE_ROOT);
    printf("============================================\n");
    printf("Server listening on port %d...\n", PORT);

    fflush(stdout);

    log_event(
        "SERVER START port=%d nid=%s",
        PORT,
        NID
    );

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_length =
            sizeof(client_address);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_address,
            &client_length
        );

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        int slot = -1;

        pthread_mutex_lock(&state_mutex);

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                slot = i;

                clients[i].fd = client_fd;
                clients[i].active = 1;
                clients[i].registered = 0;
                clients[i].username[0] = '\0';

                break;
            }
        }

        pthread_mutex_unlock(&state_mutex);

        if (slot < 0) {
            send_response(
                client_fd,
                "ERR 013 SERVER_FULL"
            );

            close(client_fd);
            continue;
        }

        log_event(
            "CONNECTION from=%s fd=%d",
            inet_ntoa(client_address.sin_addr),
            client_fd
        );

        printf(
            "New client connected from %s\n",
            inet_ntoa(client_address.sin_addr)
        );

        fflush(stdout);

        {
            pthread_t thread;
            int *argument = malloc(sizeof(int));

            if (argument == NULL) {
                close(client_fd);

                pthread_mutex_lock(&state_mutex);
                clients[slot].active = 0;
                clients[slot].fd = -1;
                pthread_mutex_unlock(&state_mutex);

                continue;
            }

            *argument = slot;

            if (
                pthread_create(
                    &thread,
                    NULL,
                    client_thread,
                    argument
                ) != 0
            ) {
                perror("pthread_create");

                free(argument);
                close(client_fd);

                pthread_mutex_lock(&state_mutex);
                clients[slot].active = 0;
                clients[slot].fd = -1;
                pthread_mutex_unlock(&state_mutex);

                continue;
            }

            pthread_detach(thread);
        }
    }

    close(server_fd);

    return EXIT_SUCCESS;
}
