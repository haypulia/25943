#ifdef __sun
#define __EXTENSIONS__
#else
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/julia_task32.sock"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigio(int sig)
{
    (void)sig;
}

static void handle_sigint(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static int write_all(int fd, const char *buffer, size_t length)
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

static int set_async(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return -1;

    if (fcntl(fd, F_SETOWN, getpid()) == -1)
        return -1;

#ifdef __sun
    flags |= FNDELAY | FASYNC;
#else
    flags |= O_NONBLOCK | O_ASYNC;
#endif

    if (fcntl(fd, F_SETFL, flags) == -1)
        return -1;

    return 0;
}

static void close_client(int clients[], int index)
{
    if (clients[index] != -1)
    {
        close(clients[index]);
        clients[index] = -1;
        printf("\nClient disconnected.\n");
        fflush(stdout);
    }
}

static void accept_clients(int server_fd, int clients[])
{
    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1)
        {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            perror("accept");
            break;
        }

        if (set_async(client_fd) == -1)
        {
            perror("fcntl client");
            close(client_fd);
            continue;
        }

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

static void process_clients(int clients[])
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] == -1)
            continue;

        while (1)
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
                    break;
                }
            }
            else if (bytes_read == 0)
            {
                close_client(clients, i);
                break;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else
            {
                perror("read");
                close_client(clients, i);
                break;
            }
        }
    }
}

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];
    struct sockaddr_un address;
    struct sigaction sa_io;
    struct sigaction sa_int;
    sigset_t block_set;
    sigset_t old_set;
    sigset_t wait_set;

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    memset(&sa_io, 0, sizeof(sa_io));
    sa_io.sa_handler = handle_sigio;
    sigemptyset(&sa_io.sa_mask);

    if (sigaction(SIGIO, &sa_io, NULL) == -1)
    {
        perror("sigaction SIGIO");
        return 1;
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction SIGINT");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGIO);
    sigaddset(&block_set, SIGINT);

    if (sigprocmask(SIG_BLOCK, &block_set, &old_set) == -1)
    {
        perror("sigprocmask");
        return 1;
    }

    wait_set = old_set;
    sigdelset(&wait_set, SIGIO);
    sigdelset(&wait_set, SIGINT);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        sigprocmask(SIG_SETMASK, &old_set, NULL);
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
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    if (set_async(server_fd) == -1)
    {
        perror("fcntl server");
        close(server_fd);
        unlink(SOCKET_PATH);
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    printf("Server 32 is waiting for clients...\n");
    fflush(stdout);

    while (!stop_requested)
    {
        sigsuspend(&wait_set);

        if (stop_requested)
            break;

        accept_clients(server_fd, clients);
        process_clients(clients);
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
        close_client(clients, i);

    close(server_fd);
    unlink(SOCKET_PATH);

    sigprocmask(SIG_SETMASK, &old_set, NULL);

    printf("\nServer 32 finished.\n");
    return 0;
}
