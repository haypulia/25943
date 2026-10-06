#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_LEN 1024

typedef struct {
    off_t offset;
    off_t length;
} Line;

void handle_sigint(int sig)
{
    (void)sig;
    printf("\nCtrl+C pressed. Program continues.\n");
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
                break;
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
        printf("%d\t%lld\t%lld\n", i + 1,
               (long long)lines[i].offset,
               (long long)lines[i].length);
    }

    while (1)
    {
        char raw[256];
        char input[100];
        int pos_input = 0;
        int valid = 1;
        int number;
        ssize_t n;
        int i;

        printf("\nEnter line number (0 - exit): ");
        fflush(stdout);

        n = read(STDIN_FILENO, raw, sizeof(raw) - 1);

        if (n <= 0)
        {
            printf("\nInput error or EOF.\n");
            break;
        }

        raw[n] = '\0';

        for (i = 0; i < n; i++)
        {
            unsigned char ch = (unsigned char)raw[i];

            if (ch == '\n' || ch == '\r')
                break;

            if (ch == 0x1B)
            {
                while (i + 1 < n && (raw[i + 1] == '[' || raw[i + 1] == 'O'))
                {
                    i++;
                    while (i + 1 < n && ((unsigned char)raw[i + 1] >= 0x20 &&
                                         (unsigned char)raw[i + 1] <= 0x7E))
                        i++;
                    break;
                }
                continue;
            }

            if (ch == 0x7F || ch == 0x08 || ch == 0x12)
            {
                if (pos_input > 0)
                {
                    pos_input--;
                    printf("\b \b");
                    fflush(stdout);
                }
                continue;
            }

            if (ch == 3)
            {
                printf("\nCtrl+C pressed.\n");
                valid = 0;
                continue;
            }

            if (iscntrl(ch))
            {
                valid = 0;
                continue;
            }

            if (!isdigit(ch))
            {
                valid = 0;
                continue;
            }

            if (pos_input < 99)
            {
                input[pos_input] = (char)ch;
                pos_input++;
                putchar(ch);
                fflush(stdout);
            }
        }

        input[pos_input] = '\0';

        if (!valid || pos_input == 0)
        {
            printf("\nPlease enter a number.\n");
            continue;
        }

        number = atoi(input);
        printf("\n");

        if (number == 0)
            break;

        if (number < 1 || number > cnt)
        {
            printf("No such line.\n");
            continue;
        }

        if (lseek(fd, lines[number - 1].offset, SEEK_SET) == -1)
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

        ssize_t bytes_read = read(fd, buffer, lines[number - 1].length);

        if (bytes_read == -1)
        {
            perror("read");
            continue;
        }

        buffer[bytes_read] = '\0';
        printf("%s\n", buffer);
    }

    close(fd);
    return 0;
}