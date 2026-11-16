#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#define SOCKET_PATH "/tmp/task31_socket"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    stop_requested = 1;
}

/* Запись всех данных в файл */
static int write_all(int fd, const char *buffer, size_t size)
{
    size_t sent = 0;

    while (sent < size)
    {
        ssize_t n = write(fd, buffer + sent, size - sent);

        if (n > 0)
            sent += (size_t)n;
        else if (n == -1 && errno == EINTR)
            continue;
        else
        {
            if (n == 0)
                errno = EIO;

            return -1;
        }
    }

    return 0;
}

/* Закрытие соединения с клиентом */
static void close_client(int clients[], int index)
{
    fprintf(stderr, "Client disconnected.\n");

    close(clients[index]);
    clients[index] = -1;
}

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];

    struct sockaddr_un address;
    struct sigaction sa;
    struct pollfd fds[MAX_CLIENTS + 1];

    /* Изначально все места свободны */
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    /* Обработка Ctrl+C */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    /* Создание сокета */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1)
    {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);

    /* Привязка сокета */
    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Ожидание подключений */
    if (listen(server_fd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    fprintf(stderr, "Server 31 is waiting for clients...\n");

    while (!stop_requested)
    {
        /* Настраиваем массив poll */
        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            fds[i + 1].fd = clients[i];
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }

        /* Ожидаем события на сокетах */
        int ready = poll(fds, MAX_CLIENTS + 1, -1);

        if (ready == -1)
        {
            if (errno == EINTR)
                continue;

            perror("poll");
            break;
        }

        /* Проверяем новые подключения */
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
                int added = 0;

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i] == -1)
                    {
                        clients[i] = client_fd;
                        added = 1;
                        break;
                    }
                }

                if (added)
                    fprintf(stderr, "Client connected.\n");
                else
                {
                    fprintf(stderr,
                            "Maximum number of clients reached.\n");
                    close(client_fd);
                }
            }
        }

        /* Проверяем данные от подключённых клиентов */
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            short events = fds[i + 1].revents;

            if (clients[i] == -1 ||
                !(events & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
                continue;

            char buffer[BUFFER_SIZE];

            ssize_t bytes = read(clients[i], buffer, sizeof(buffer));

            if (bytes > 0)
            {
                /* Переводим текст в верхний регистр */
                for (ssize_t j = 0; j < bytes; j++)
                {
                    buffer[j] =
                        (char)toupper((unsigned char)buffer[j]);
                }

                /* Выводим текст в терминал сервера */
                if (write_all(STDOUT_FILENO,
                              buffer, (size_t)bytes) == -1)
                {
                    perror("write");
                    stop_requested = 1;
                }
            }
            else if (bytes == 0)
            {
                close_client(clients, i);
            }
            else if (errno == EINTR ||
                     errno == EAGAIN ||
                     errno == EWOULDBLOCK)
            {
                continue;
            }
            else
            {
                perror("read");
                close(clients[i]);
                clients[i] = -1;
            }
        }
    }

    /* Завершение работы */
    fprintf(stderr, "Server 31 is stopping...\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != -1)
            close(clients[i]);
    }

    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}