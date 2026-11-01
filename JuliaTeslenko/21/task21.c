#include <stdio.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t count = 0;
volatile sig_atomic_t quit = 0;

// Обработчик SIGINT (Ctrl+C)
void handle_sigint(int sig)
{
    (void)sig;
    count++;
    write(STDOUT_FILENO, "\a", 1);
}
// Обработчик SIGQUIT (Ctrl+\)
void handle_sigquit(int sig)
{
    (void)sig;
    quit = 1;
}

int main(void)
{
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);

    printf("Program started.\n");
    printf("Press Ctrl+C to beep, Ctrl+\\ to exit.\n");

    while (!quit)
    {
        pause();
    }

    printf("\nNumber of beeps: %d\n", (int)count);
    printf("Program finished.\n");

    return 0;
}