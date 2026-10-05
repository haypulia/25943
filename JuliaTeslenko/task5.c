#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_LINES 1000
#define MAX_LEN 1024

typedef struct {
    off_t offset;
    off_t length;
} Line;

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
        int number;

        printf("\nEnter line number (0 - exit): ");
        scanf("%d", &number);

        if (number == 0) 
        {
            break;
        }

        if (number < 1 || number > cnt) 
        {
            printf("No such line\n");
            continue;
        }

        lseek(fd, lines[number - 1].offset, SEEK_SET);

        char buffer[MAX_LEN];

        if (lines[number - 1].length >= MAX_LEN) 
        {
            printf("The string is too long.\n");
            continue;
        }

        read(fd, buffer, lines[number - 1].length);

        buffer[lines[number - 1].length] = '\0';

        printf("%s\n", buffer);
    }

    close(fd);

    return 0;
}