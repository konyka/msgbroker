/* T-JITTER: validate mb_reconnect_next_ivl_jittered()
 *
 * Contract:
 *   - Wraps mb_reconnect_next_ivl() doubling+capping logic.
 *   - Adds +/- 25% uniform random jitter on top of the doubled value.
 *   - Result is clamped to ivl_max.
 *   - Uses a thread-local PRNG seeded from mb_clock_us(), so concurrent
 *     threads observe independent streams.
 *
 * Statistical gate (ivl=100, ivl_max=1000, N=1000):
 *   - Base doubled value: min(100*2, 1000) = 200.
 *   - Jitter band: 200 * +/- 25% = [150, 250].
 *   - After clamp to 1000: all values must lie in [150, 1000].
 *   - At least 80% of returned values must differ from the unclamped,
 *     no-jitter base (200), proving the PRNG is actually wired in.
 *
 * NOTE: bounds below are intentionally loose (use [150, 1000] which is a
 * subset of the strict +/-25% band [150,250]) so the test does not flake
 * on round-to-int boundaries; the strict band is verified separately.
 */
#include "../../src/pal/sleep.h"
#include "../../src/pal/clock.h"

#include <stdio.h>
#include <assert.h>

#define ITERATIONS 1000
#define IVL        100
#define IVL_MAX    1000
#define JITTER_PCT 25
#define MIN_DIFF   80  /* at least 80% of samples must deviate from base */

/* Strict +/- 25% band around the doubled value, then clamped to ivl_max. */
static int expected_min (int ivl, int ivl_max)
{
    int doubled;
    int band;

    if (ivl_max > 0 && ivl > ivl_max)
        doubled = ivl_max;
    else if (ivl >= ivl_max)
        doubled = ivl_max;
    else if (ivl > ivl_max / 2)
        doubled = ivl_max;
    else
        doubled = ivl * 2;

    band = (doubled * JITTER_PCT) / 100;
    if (band < 1)
        band = 1;
    return doubled - band;
}

static int expected_max (int ivl, int ivl_max)
{
    int doubled;
    int band;

    if (ivl_max > 0 && ivl > ivl_max)
        doubled = ivl_max;
    else if (ivl >= ivl_max)
        doubled = ivl_max;
    else if (ivl > ivl_max / 2)
        doubled = ivl_max;
    else
        doubled = ivl * 2;

    band = (doubled * JITTER_PCT) / 100;
    if (band < 1)
        band = 1;
    int hi = doubled + band;
    if (ivl_max > 0 && hi > ivl_max)
        hi = ivl_max;
    return hi;
}

int main (void)
{
    int base_doubled;
    int lo;
    int hi;
    int diff_count = 0;
    int i;

    /* Sanity-check our expected band mirrors the production logic. */
    base_doubled = mb_reconnect_next_ivl (IVL, IVL_MAX);
    assert (base_doubled == 200);

    lo = expected_min (IVL, IVL_MAX);
    hi = expected_max (IVL, IVL_MAX);
    assert (lo <= hi);
    /* Clamped upper bound must not exceed ivl_max. */
    assert (hi <= IVL_MAX);
    /* Lower bound is doubled minus 25% of doubled (rounded down). */
    assert (lo >= base_doubled - (base_doubled * JITTER_PCT) / 100);

    printf ("test_jitter: expected band [%d, %d], base doubled=%d\n",
        lo, hi, base_doubled);

    /* Statistical distribution check. */
    for (i = 0; i < ITERATIONS; i++) {
        int v = mb_reconnect_next_ivl_jittered (IVL, IVL_MAX);
        assert (v >= lo && v <= hi);
        if (v != base_doubled)
            diff_count++;
    }
    assert (diff_count * 100 >= ITERATIONS * MIN_DIFF);

    printf ("test_jitter: %d/%d samples differ from base (%d%% >= %d%%)\n",
        diff_count, ITERATIONS,
        (diff_count * 100) / ITERATIONS, MIN_DIFF);

    /* Edge cases. */

    /* ivl_max <= 0 => no capping path. Wrapper should still safely fall
     * back to a deterministic-ish (possibly clamped-to-INT_MAX) value. */
    int v0 = mb_reconnect_next_ivl_jittered (IVL, 0);
    assert (v0 > 0);

    /* Already at the cap: doubling returns ivl_max, jitter must NOT
     * exceed ivl_max (clamp must hold even on the upper side). */
    int v_cap = mb_reconnect_next_ivl_jittered (IVL_MAX, IVL_MAX);
    assert (v_cap >= IVL_MAX - (IVL_MAX * JITTER_PCT) / 100);
    assert (v_cap <= IVL_MAX);

    /* Above the cap (misconfigured ivl > ivl_max): must clamp to max,
     * jitter must not push above max. */
    int v_over = mb_reconnect_next_ivl_jittered (IVL_MAX * 2, IVL_MAX);
    assert (v_over >= 0);
    assert (v_over <= IVL_MAX);

    /* ivl <= 0: defensive — must not return a wild negative or wrap. */
    int v_neg = mb_reconnect_next_ivl_jittered (0, IVL_MAX);
    assert (v_neg >= 0 && v_neg <= IVL_MAX);

    printf ("test_jitter: PASSED (clock_us=%llu)\n",
        (unsigned long long) mb_clock_us ());
    return 0;
}