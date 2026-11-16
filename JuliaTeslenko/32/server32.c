#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef __sun
#include <sys/file.h>
#endif

#define SOCKET_PATH "/tmp/task32_socket"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t sigio_received = 0;
volatile sig_atomic_t stop_requested = 0;

/* Обработчик SIGIO */
static void handle_sigio(int sig)
{
    (void)sig;
    sigio_received = 1;
}

/* Обработчик Ctrl+C */
static void handle_sigint(int sig)
{
    (void)sig;
    stop_requested = 1;
}

/* Включение асинхронного режима */
static int set_async(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return -1;
    }

    /* Указываем процесс, которому отправлять SIGIO */
    if (fcntl(fd, F_SETOWN, (int)getpid()) == -1)
    {
        perror("fcntl F_SETOWN");
        return -1;
    }

#ifdef __sun
    flags |= FNDELAY | FASYNC;
#else
    flags |= O_NONBLOCK | O_ASYNC;
#endif

    if (fcntl(fd, F_SETFL, flags) == -1)
    {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

/* Запись всех данных */
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

/* Закрытие клиента */
static void close_client(int clients[], int index)
{
    fprintf(stderr, "Client disconnected.\n");

    close(clients[index]);
    clients[index] = -1;
}

/* Приём новых клиентов */
static void accept_clients(int server_fd, int clients[])
{
    while (!stop_requested)
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

        int added = 0;
        int free_slot_found = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] == -1)
            {
                free_slot_found = 1;

                /* Настраиваем асинхронный режим клиента */
                if (set_async(client_fd) == 0)
                {
                    clients[i] = client_fd;
                    added = 1;

                    fprintf(stderr, "Client connected.\n");
                }

                break;
            }
        }

        if (!added)
        {
            if (!free_slot_found)
                fprintf(stderr,
                        "Maximum number of clients reached.\n");

            close(client_fd);
        }
    }
}

/* Получение и обработка данных от всех клиентов */
static void process_clients(int clients[])
{
    int data_was_read;

    do
    {
        data_was_read = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] == -1)
                continue;

            char buffer[BUFFER_SIZE];

            ssize_t bytes = read(clients[i], buffer, sizeof(buffer));

            if (bytes > 0)
            {
                data_was_read = 1;

                /* Переводим символы в верхний регистр */
                for (ssize_t j = 0; j < bytes; j++)
                {
                    buffer[j] =
                        (char)toupper((unsigned char)buffer[j]);
                }

                /* Выводим результат */
                if (write_all(STDOUT_FILENO,
                              buffer, (size_t)bytes) == -1)
                {
                    perror("write");
                    stop_requested = 1;
                    return;
                }
            }
            else if (bytes == 0)
            {
                close_client(clients, i);
            }
            else if (errno == EINTR)
            {
                data_was_read = 1;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Пока данных больше нет */
            }
            else
            {
                perror("read");
                close(clients[i]);
                clients[i] = -1;
            }
        }

    } while (data_was_read && !stop_requested);
}

int main(void)
{
    int server_fd;
    int clients[MAX_CLIENTS];

    struct sockaddr_un address;
    struct sigaction sa;

    sigset_t block_set;
    sigset_t old_set;
    sigset_t wait_set;

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    /* Блокируем SIGIO */
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGIO);

    if (sigprocmask(SIG_BLOCK, &block_set, &old_set) == -1)
    {
        perror("sigprocmask");
        return 1;
    }

    wait_set = old_set;
    sigdelset(&wait_set, SIGIO);
    sigdelset(&wait_set, SIGINT);

    /* Устанавливаем обработчик SIGIO */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigio;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGIO, &sa, NULL) == -1)
    {
        perror("sigaction SIGIO");
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    /* Устанавливаем обработчик SIGINT */
    sa.sa_handler = handle_sigint;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    /* Создание сокета */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1)
    {
        perror("socket");
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1)
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

    /* Включаем асинхронный режим серверного сокета */
    if (set_async(server_fd) == -1)
    {
        close(server_fd);
        unlink(SOCKET_PATH);
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        return 1;
    }

    fprintf(stderr, "Server 32 is waiting for clients...\n");

    while (!stop_requested)
    {
        /* Ждём поступления SIGIO */
        while (!sigio_received && !stop_requested)
        {
            sigsuspend(&wait_set);
        }

        if (stop_requested)
            break;

        sigio_received = 0;

        /* Принимаем новые подключения */
        accept_clients(server_fd, clients);

        /* Обрабатываем данные клиентов */
        process_clients(clients);
    }

    fprintf(stderr, "Server 32 is stopping...\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != -1)
            close(clients[i]);
    }

    close(server_fd);
    unlink(SOCKET_PATH);

    sigprocmask(SIG_SETMASK, &old_set, NULL);

    return 0;
}