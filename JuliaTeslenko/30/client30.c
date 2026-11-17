#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <termios.h>
#include <poll.h>

#define SOCKET_PATH "/tmp/task30_socket"

// Игнорируем последовательности от стрелочек
void ignore_escape_sequence(void)
{
    char ch;
    struct pollfd pfd;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    // Читаем следующий символ
    if (poll(&pfd, 1, 50) <= 0)
        return;

    if (read(STDIN_FILENO, &ch, 1) != 1)
        return;

    if (ch == '[' || ch == 'O')
    {
        while (poll(&pfd, 1, 50) > 0)
        {
            if (read(STDIN_FILENO, &ch, 1) != 1)
                break;

            if (ch >= 0x40 && ch <= 0x7E)
                break;
        }
    }
}

// Отправляем весь текст
int send_all(int fd, const char *buffer, size_t len)
{
    size_t sent = 0;

    while (sent < len)
    {
        ssize_t n = write(fd, buffer + sent, len - sent);

        if (n <= 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

int main(void)
{
    int sock_fd;
    struct sockaddr_un address;
    struct termios old_term, new_term;
    char buffer[1024];
    char ch;
    size_t len = 0;

    // Создаём сокет
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1)
    {
        perror("socket");
        return 1;
    }

    // Указываем адрес сервера
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    // Подключаемся к серверу
    socklen_t addr_len = strlen(address.sun_path) + sizeof(address.sun_family);
    if (connect(sock_fd, (struct sockaddr *)&address, addr_len) == -1)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to server.\n");
    printf("Enter text: ");
    fflush(stdout);

    // Сохраняем настройки терминала
    if (tcgetattr(STDIN_FILENO, &old_term) == -1)
    {
        perror("tcgetattr");
        close(sock_fd);
        return 1;
    }

    new_term = old_term;

    // Отключаем обычный режим ввода и отображение символов
    new_term.c_lflag &= ~(ICANON | ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1)
    {
        perror("tcsetattr");
        close(sock_fd);
        return 1;
    }

    // Вводим текст с клавиатуры
    while (len < sizeof(buffer) - 1)
    {
        if (read(STDIN_FILENO, &ch, 1) != 1)
            break;

        // Обрабатываем стрелочки
        if (ch == '\033')
        {
            ignore_escape_sequence();
            continue;
        }

        // Enter завершает ввод
        if (ch == '\n' || ch == '\r')
        {
            buffer[len++] = '\n';
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        // Backspace удаляет последний символ
        if (ch == 127 || ch == 8)
        {
            if (len > 0)
            {
                len--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }

        // Ctrl+D завершает ввод
        if (ch == 4)
            break;

        // Записываем обычный символ
        buffer[len++] = ch;
        write(STDOUT_FILENO, &ch, 1);
    }

    // Если ничего не ввели, используем демонстрационный текст
    if (len == 0)
    {
        const char *demo_text =
            "Hello Unix Domain Socket!\n"
            "This is a demonstration of task 30.\n";

        len = strlen(demo_text);
        memcpy(buffer, demo_text, len);

        printf("\nDemo mode:\n");
        printf("%s", demo_text);
    }


    // Восстанавливаем настройки терминала
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) == -1)
        perror("tcsetattr");

    // Отправляем текст серверу
    if (send_all(sock_fd, buffer, len) == -1)
    {
        perror("write");
        close(sock_fd);
        return 1;
    }

    printf("Text sent.\n");

    // Закрываем соединение
    close(sock_fd);

    printf("Client finished.\n");

    return 0;
}