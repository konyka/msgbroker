#include "sleep.h"

#if defined _WIN32
#include "win.h"
#else
#include <time.h>
#endif

#include "clock.h"

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

void mb_msleep_while (mb_atomic_int *running, int milliseconds)
{
    int waited = 0;

    if (milliseconds <= 0)
        return;

    while (waited < milliseconds) {
        int slice;

        if (running && !mb_atomic_load (running))
            return;
        slice = milliseconds - waited;
        if (slice > 50)
            slice = 50;
        mb_msleep (slice);
        waited += slice;
    }
}

int mb_reconnect_cap_ivl (int ivl, int ivl_max)
{
    if (ivl_max > 0 && ivl > ivl_max)
        return ivl_max;
    return ivl;
}

int mb_reconnect_next_ivl (int current_ivl, int ivl_max)
{
    if (ivl_max <= 0)
        return current_ivl;
    /* Cap even when current already exceeds max (misconfigured ivl > max). */
    if (current_ivl >= ivl_max)
        return ivl_max;
    if (current_ivl > ivl_max / 2)
        return ivl_max;
    return current_ivl * 2;
}

/* xorshift32 PRNG state, one slot per thread. Seeded lazily from
 * mb_clock_us() so the first call from any thread produces a
 * thread-independent stream. Lock-free; state is private to the thread. */
#if defined _WIN32
#define MB_TLS __declspec(thread)
#else
#define MB_TLS __thread
#endif

static MB_TLS unsigned int mb_jitter_state = 0;

static unsigned int mb_jitter_next (void)
{
    /* xorshift32 (Marsaglia). Period 2^32-1; adequate for jitter, not crypto. */
    unsigned int x = mb_jitter_state;
    if (x == 0) {
        /* Mix the seed: xor with the upper 32 bits of mb_clock_us() so two
         * threads spawning in the same microsecond still diverge. */
        uint64_t s = mb_clock_us ();
        x = (unsigned int) (s ^ (s >> 32) ^ 0x9E3779B9u);
        if (x == 0)
            x = 0x9E3779B9u;
        mb_jitter_state = x;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mb_jitter_state = x;
    return x;
}

int mb_reconnect_next_ivl_jittered (int current_ivl, int ivl_max)
{
    int base = mb_reconnect_next_ivl (current_ivl, ivl_max);
    int band;
    int jitter;
    int result;

    if (base <= 0)
        return base;  /* mb_reconnect_next_ivl() returned ivl directly when
                       * ivl_max<=0 or ivl<=0; let the caller handle that. */

    band = (base * 25) / 100;  /* +/- 25% of base, in milliseconds. */
    if (band < 1)
        band = 1;  /* Always allow at least 1ms of jitter for tiny bases. */

    /* Uniform jitter in [-band, +band]. The PRNG returns a 32-bit value;
     * take the low bits so the distribution is well-spread. */
    jitter = (int) (mb_jitter_next () % (unsigned int) (band * 2 + 1)) - band;

    result = base + jitter;
    if (result < 0)
        result = 0;  /* Shouldn't happen for positive base, but defensive. */
    if (ivl_max > 0 && result > ivl_max)
        result = ivl_max;

    return result;
}
