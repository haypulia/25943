#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/task30_socket"

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_un address;
    char buffer[1024];
    ssize_t bytes;
    socklen_t addr_len;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    addr_len = (socklen_t)(strlen(address.sun_path) + sizeof(address.sun_family));

    /* Проверяем, не работает ли уже сервер на этом сокете */
    int probe_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (probe_fd != -1) {
        if (connect(probe_fd, (struct sockaddr *)&address, addr_len) == 0) {
            fprintf(stderr, "Server is already running on %s\n", SOCKET_PATH);
            close(probe_fd);
            close(server_fd);
            return 1;
        }
        close(probe_fd);
    }

    /* Файл мёртвый (или его нет) — удаляем и занимаем путь */
    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&address, addr_len) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Server is waiting for a client...\n");
    fflush(stdout);

    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Client connected.\n");
    printf("Uppercase text: ");
    fflush(stdout);

    while ((bytes = read(client_fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes; i++)
            buffer[i] = (char)toupper((unsigned char)buffer[i]);
        write(STDOUT_FILENO, buffer, (size_t)bytes);
    }

    printf("\nClient disconnected.\n");

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    printf("Server finished.\n");
    return 0;
}