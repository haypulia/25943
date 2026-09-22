#include <stdio.h>
#include <time.h>

int main(void)
{
    time_t t;
    struct tm *tm;

    time(&t);
    tm = gmtime(&t);

    tm->tm_hour -= 8;

    if (tm->tm_hour < 0) {
        tm->tm_hour += 24;
        tm->tm_mday--;
    }

    printf("%s", asctime(tm));

    return 0;
}
