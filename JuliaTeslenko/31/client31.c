#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task31_socket"
#define BUFFER_SIZE 1024

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

int main(void)
{
    int sock_fd;
    struct sockaddr_un address;

    signal(SIGPIPE, SIG_IGN);

    /* Создаём сокет */
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1)
    {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    /* Подключаемся к серверу */
    if (connect(sock_fd, (struct sockaddr *)&address,
                sizeof(address)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to server 31.\n");
    printf("Enter text (Ctrl+D to disconnect):\n");

    char buffer[BUFFER_SIZE];

    /* Читаем текст и отправляем серверу */
    while (1)
    {
        ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (bytes > 0)
        {
            if (write_all(sock_fd, buffer, (size_t)bytes) == -1)
            {
                perror("write");
                close(sock_fd);
                return 1;
            }
        }
        else if (bytes == 0)
        {
            break;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else
        {
            perror("read");
            close(sock_fd);
            return 1;
        }
    }

    shutdown(sock_fd, SHUT_WR);
    close(sock_fd);

    printf("Disconnected from server 31.\n");

    return 0;
}