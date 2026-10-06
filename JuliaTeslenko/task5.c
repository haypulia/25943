#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <termios.h>

#define MAX_LINES 1000
#define MAX_LEN 1024

typedef struct {
    off_t offset;
    off_t length;
} Line;

struct termios old_terminal;

void restore_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);
}

void handle_sigint(int sig)
{
    (void)sig;
    printf("\nCtrl+C pressed. Program continues.\n");
}

int read_line_number()
{
    char input[100];
    int pos = 0;
    char ch;
    int valid = 1;

    while (1)
    {
        if (read(STDIN_FILENO, &ch, 1) != 1)
        {
            continue;
        }

        if ((unsigned char)ch == 3)
        {
            continue;
        }

        if (ch == '\n' || ch == '\r')
        {
            printf("\n");
            break;
        }

        if (ch == 8 || (unsigned char)ch == 127)
        {
            if (pos > 0)
            {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }

            continue;
        }

        if ((unsigned char)ch == 27)
        {
            char next;

            if (read(STDIN_FILENO, &next, 1) != 1)
            {
                continue;
            }

            if (next == '[')
            {
                if (read(STDIN_FILENO, &next, 1) != 1)
                {
                    continue;
                }
            }

            continue;
        }

        if ((unsigned char)ch == 11)
        {
            continue;
        }

        if (isdigit((unsigned char)ch))
        {
            if (pos < 99)
            {
                input[pos] = ch;
                pos++;

                printf("%c", ch);
                fflush(stdout);
            }

            continue;
        }

        valid = 0;

        printf("\a");
        fflush(stdout);
    }

    if (pos == 0)
    {
        printf("Please enter a number.\n");
        return -1;
    }

    if (!valid)
    {
        printf("Please enter a number.\n");
        return -1;
    }

    input[pos] = '\0';

    return atoi(input);
}

int main(int argc, char *argv[])
{
    int fd;
    char c;
    Line lines[MAX_LINES];
    int cnt = 0;
    off_t line_start = 0;
    off_t pos;

    if (argc != 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_sigint);

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

    if (tcgetattr(STDIN_FILENO, &old_terminal) == -1)
    {
        perror("tcgetattr");
        close(fd);
        return 1;
    }

    struct termios new_terminal = old_terminal;

    new_terminal.c_lflag &= ~(ICANON | ECHO);
    new_terminal.c_cc[VMIN] = 1;
    new_terminal.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal) == -1)
    {
        perror("tcsetattr");
        close(fd);
        return 1;
    }

    atexit(restore_terminal);

    while (1)
    {
        int number;

        printf("\nEnter line number (0 - exit): ");
        fflush(stdout);

        number = read_line_number();

        if (number == -1)
        {
            continue;
        }

        if (number == 0)
        {
            break;
        }

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

        ssize_t bytes_read;

        bytes_read = read(fd,
                          buffer,
                          lines[number - 1].length);

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