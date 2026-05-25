#include "../../src/pal/clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    uint64_t t1 = mb_clock_now ();
    assert (t1 > 0);

    uint64_t t2 = mb_clock_us ();
    assert (t2 > 0);

    uint64_t t3 = mb_clock_ms ();
    assert (t3 > 0);

    assert (t2 >= t1 || t2 / 1000 == t3 / 1000);

    printf ("test_clock: PASSED (now=%llu us=%llu ms=%llu)\n",
        (unsigned long long) t1, (unsigned long long) t2,
        (unsigned long long) t3);
    return 0;
}
