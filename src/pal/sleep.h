#ifndef MB_SLEEP_H_INCLUDED
#define MB_SLEEP_H_INCLUDED

#include "atomic.h"

void mb_sleep (int seconds);
void mb_msleep (int milliseconds);

/* Sleep up to milliseconds, returning early when *running becomes 0.
 * Used by reconnect backoff so stop()/join is not blocked for the full ivl. */
void mb_msleep_while (mb_atomic_int *running, int milliseconds);

/* Cap initial reconnect_ivl to reconnect_ivl_max (0 max = uncapped). */
int mb_reconnect_cap_ivl (int ivl, int ivl_max);

/* Double reconnect backoff without signed overflow; clamp to ivl_max. */
int mb_reconnect_next_ivl (int current_ivl, int ivl_max);

/* Same as mb_reconnect_next_ivl() but adds +/- 25% uniform random jitter
 * on top of the doubled value, then clamps to ivl_max. Used by reconnect
 * loops to avoid thundering herd after a shared outage. Backed by a
 * thread-local PRNG seeded from mb_clock_us(); safe for concurrent calls. */
int mb_reconnect_next_ivl_jittered (int current_ivl, int ivl_max);

#endif
