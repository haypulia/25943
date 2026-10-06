#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINES 1000
#define MAX_LEN 1024

typedef struct {
    off_t offset;
    off_t length;
} Line;

volatile sig_atomic_t ctrl_c_pressed = 0;


/*
 * Обработчик Ctrl+C
 */
void handle_sigint(int sig)
{
    (void)sig;
    ctrl_c_pressed = 1;
}


int main(int argc, char *argv[])
{
    int fd;
    char c;
    Line lines[MAX_LINES];
    int cnt = 0;
    off_t line_start = 0;
    off_t pos;
    signal(SIGINT, handle_sigint);


    if (argc != 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }


    fd = open(argv[1], O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    while (read(fd, &c, 1) == 1)
    {
        if (c == '\n')
        {
            pos = lseek(fd, 0L, SEEK_CUR);

            lines[cnt].offset = line_start;
            lines[cnt].length = pos - line_start - 1;

            cnt++;

            line_start = pos;

            if (cnt >= MAX_LINES)
            {
                break;
            }
        }
    }

    pos = lseek(fd, 0L, SEEK_CUR);

    if (line_start < pos && cnt < MAX_LINES)
    {
        lines[cnt].offset = line_start;
        lines[cnt].length = pos - line_start;
        cnt++;
    }
    printf("\nLine table:\n");
    printf("Line\tOffset\tLength\n");

    for (int i = 0; i < cnt; i++)
    {
        printf("%d\t%lld\t%lld\n",
               i + 1,
               (long long)lines[i].offset,
               (long long)lines[i].length);
    }
    while (1)
    {
        char input[100];
        ssize_t bytes_read;
        int valid = 1;
        int number;


        printf("\nEnter line number (0 - exit): ");
        fflush(stdout);

        bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);


        if (ctrl_c_pressed)
        {
            ctrl_c_pressed = 0;

            printf("\n");
            continue;
        }

        if (bytes_read < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("read");
            break;
        }


        if (bytes_read == 0)
        {
            break;
        }


        input[bytes_read] = '\0';

        if (input[bytes_read - 1] == '\n')
        {
            input[bytes_read - 1] = '\0';
            bytes_read--;
        }
        if (bytes_read == 0)
        {
            printf("Please enter a number.\n");
            continue;
        }

        for (ssize_t i = 0; i < bytes_read; i++)
        {
            unsigned char ch = input[i];

            if (ch == 27)
            {
                valid = 0;
                break;
            }


            if (!isdigit(ch))
            {
                valid = 0;
                break;
            }
        }


        if (!valid)
        {
            printf("Please enter a number.\n");
            continue;
        }

        number = atoi(input);

        if (number == 0)
        {
            break;
        }

        if (number < 1 || number > cnt)
        {
            printf("No such line.\n");
            continue;
        }
        if (lseek(fd,
                  lines[number - 1].offset,
                  SEEK_SET) == -1)
        {
            perror("lseek");
            continue;
        }


        char buffer[MAX_LEN];


        if (lines[number - 1].length >= MAX_LEN)
        {
            printf("The string is too long.\n");
            continue;
        }

        ssize_t line_bytes;

        line_bytes = read(fd,
                          buffer,
                          lines[number - 1].length);


        if (line_bytes == -1)
        {
            perror("read");
            continue;
        }


        buffer[line_bytes] = '\0';

        printf("%s\n", buffer);
    }


    close(fd);

    return 0;
}