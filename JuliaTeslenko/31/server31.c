#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task31_socket"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t stop_requested = 0;

void handle_sigint(int sig)
{
    (void)sig;
    stop_requested = 1;
}

int write_all(int fd, const char *buffer, size_t length)
{
    size_t sent = 0;

    while (sent < length)
    {
        ssize_t n = write(fd, buffer + sent, length - sent);

        if (n > 0)
            sent += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else
            return -1;
    }

    return 0;
}

void close_client(int clients[], int index)
{
    if (clients[index] != -1)
    {
        close(clients[index]);
        clients[index] = -1;
        printf("\nClient disconnected.\n");
        fflush(stdout);
    }
}

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];
    struct sockaddr_un address;
    struct sigaction sa;

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Server 31 is waiting for clients...\n");
    fflush(stdout);

    while (!stop_requested)
    {
        struct pollfd fds[MAX_CLIENTS + 1];

        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            fds[i + 1].fd = clients[i];
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }

        int result = poll(fds, MAX_CLIENTS + 1, -1);

        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd == -1)
            {
                if (errno != EINTR)
                    perror("accept");
            }
            else
            {
                int placed = 0;

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i] == -1)
                    {
                        clients[i] = client_fd;
                        placed = 1;
                        printf("Client connected.\n");
                        fflush(stdout);
                        break;
                    }
                }

                if (!placed)
                {
                    printf("Maximum number of clients reached.\n");
                    close(client_fd);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] == -1)
                continue;

            short events = fds[i + 1].revents;

            if (events & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
            {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(clients[i], buffer, sizeof(buffer));

                if (bytes_read > 0)
                {
                    for (ssize_t j = 0; j < bytes_read; j++)
                        buffer[j] = (char)toupper((unsigned char)buffer[j]);

                    if (write_all(STDOUT_FILENO, buffer, (size_t)bytes_read) == -1)
                    {
                        perror("write");
                        close_client(clients, i);
                    }
                }
                else if (bytes_read == 0)
                {
                    close_client(clients, i);
                }
                else if (errno != EINTR)
                {
                    perror("read");
                    close_client(clients, i);
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
        close_client(clients, i);

    close(server_fd);
    unlink(SOCKET_PATH);

    printf("\nServer 31 finished.\n");
    return 0;
}