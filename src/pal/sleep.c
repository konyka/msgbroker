#include "sleep.h"

#if defined _WIN32
#include "win.h"
#else
#include <time.h>
#endif

void mb_sleep (int seconds)
{
#if defined _WIN32
    Sleep ((DWORD) seconds * 1000);
#else
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    nanosleep (&ts, NULL);
#endif
}

void mb_msleep (int milliseconds)
{
#if defined _WIN32
    Sleep ((DWORD) milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep (&ts, NULL);
#endif
}

void mb_msleep_while (volatile int *running, int milliseconds)
{
    int waited = 0;

    if (milliseconds <= 0)
        return;

    while (waited < milliseconds) {
        int slice;

        if (running && !*running)
            return;
        slice = milliseconds - waited;
        if (slice > 50)
            slice = 50;
        mb_msleep (slice);
        waited += slice;
    }
}
