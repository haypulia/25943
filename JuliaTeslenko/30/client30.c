#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>

#define SOCKET_PATH "/tmp/task30_socket"

/* Неблокирующее чтение escape-последовательности (без задержек) */
static void ignore_escape_sequence(void)
{
    char ch;

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch >= 0x40 && ch <= 0x7E)
            break;
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);
}

static int send_all(int fd, const char *buffer, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buffer + sent, len - sent);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* Подключение с несколькими попытками — спасает при одновременном запуске */
static int connect_with_retry(int fd, struct sockaddr_un *addr, socklen_t len)
{
    int attempts = 50; /* ~1 секунда суммарно */
    while (attempts-- > 0) {
        if (connect(fd, (struct sockaddr *)addr, len) == 0)
            return 0;
        if (errno != ENOENT && errno != ECONNREFUSED)
            return -1;
        struct timespec ts = {0, 20000000L}; /* 20 мс */
        nanosleep(&ts, NULL);
    }
    return -1;
}

int main(void)
{
    int sock_fd;
    struct sockaddr_un address;
    struct termios old_term, new_term;
    char buffer[1024];
    char ch;
    size_t len = 0;
    socklen_t addr_len;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    addr_len = (socklen_t)(strlen(address.sun_path) + sizeof(address.sun_family));

    if (connect_with_retry(sock_fd, &address, addr_len) == -1) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to server.\n");
    printf("Enter text: ");
    fflush(stdout);

    if (tcgetattr(STDIN_FILENO, &old_term) == -1) {
        perror("tcgetattr");
        close(sock_fd);
        return 1;
    }

    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1) {
        perror("tcsetattr");
        close(sock_fd);
        return 1;
    }

    /* Сбрасываем накопленный ввод, чтобы старые Enter не влияли */
    tcflush(STDIN_FILENO, TCIFLUSH);

    while (len < sizeof(buffer) - 1) {
        if (read(STDIN_FILENO, &ch, 1) != 1)
            break;

        if (ch == '\033') {
            ignore_escape_sequence();
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            buffer[len++] = '\n';
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        if (ch == 127 || ch == 8) {
            if (len > 0) {
                len--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }

        if (ch == 4) /* Ctrl+D */
            break;

        buffer[len++] = ch;
        write(STDOUT_FILENO, &ch, 1);
    }

    if (len == 0) {
        const char *demo_text =
            "Hello Unix Domain Socket!\n"
            "This is a demonstration of task 30.\n";
        len = strlen(demo_text);
        memcpy(buffer, demo_text, len);
        printf("\nDemo mode:\n%s", demo_text);
    }

    /* Сбрасываем хвост ввода и восстанавливаем терминал */
    tcflush(STDIN_FILENO, TCIFLUSH);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) == -1)
        perror("tcsetattr");

    if (send_all(sock_fd, buffer, len) == -1) {
        perror("write");
        close(sock_fd);
        return 1;
    }

    printf("Text sent.\n");
    close(sock_fd);
    printf("Client finished.\n");
    return 0;
}