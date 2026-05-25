#include "clock.h"

#if defined _WIN32
#include "win.h"
#else
#include <time.h>
#endif

uint64_t mb_clock_now (void)
{
#if defined _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER count;
    QueryPerformanceFrequency (&freq);
    QueryPerformanceCounter (&count);
    return (uint64_t) (count.QuadPart * 1000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000ULL + (uint64_t) (ts.tv_nsec / 1000);
#endif
}

uint64_t mb_clock_us (void)
{
    return mb_clock_now ();
}

uint64_t mb_clock_ms (void)
{
    return mb_clock_now () / 1000ULL;
}
