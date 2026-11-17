#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task32_socket"
#define BUFFER_SIZE 1024

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

int main(void)
{
    int sock_fd;
    struct sockaddr_un address;
    char buffer[BUFFER_SIZE];

    signal(SIGPIPE, SIG_IGN);

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to server 32.\n");
    printf("Enter text (Ctrl+D to disconnect):\n");

    while (1)
    {
        ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (bytes_read > 0)
        {
            if (write_all(sock_fd, buffer, (size_t)bytes_read) == -1)
            {
                perror("write");
                break;
            }
        }
        else if (bytes_read == 0)
        {
            break;
        }
        else if (errno != EINTR)
        {
            perror("read");
            break;
        }
    }

    shutdown(sock_fd, SHUT_WR);
    close(sock_fd);

    printf("\nDisconnected from server 32.\n");
    return 0;
}