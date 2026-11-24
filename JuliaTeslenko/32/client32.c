#ifdef __sun
#define __EXTENSIONS__
#else
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/julia_task32.sock"
#define BUFFER_SIZE 1024


static size_t remove_escape_sequences(char *buffer)
{
    size_t read_pos = 0;
    size_t write_pos = 0;
    size_t length = strlen(buffer);

    while (read_pos < length)
    {
        if ((unsigned char)buffer[read_pos] == 0x1B)
        {
            read_pos++;

            if (read_pos < length && buffer[read_pos] == '[')
            {
                read_pos++;

                while (read_pos < length)
                {
                    unsigned char ch = (unsigned char)buffer[read_pos++];

                    if (ch >= 0x40 && ch <= 0x7E)
                        break;
                }
            }
            else if (read_pos < length && buffer[read_pos] == 'O')
            {
                read_pos += 2;
            }

            continue;
        }

        buffer[write_pos++] = buffer[read_pos++];
    }

    buffer[write_pos] = '\0';
    return write_pos;
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

    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        if (write_all(sock_fd, buffer, strlen(buffer)) == -1)
        {
            perror("write");
            break;
        }
    }

    shutdown(sock_fd, SHUT_WR);
    close(sock_fd);

    printf("\nDisconnected from server 32.\n");
    return 0;
}
