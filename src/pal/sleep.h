#ifndef MB_SLEEP_H_INCLUDED
#define MB_SLEEP_H_INCLUDED

void mb_sleep (int seconds);
void mb_msleep (int milliseconds);

/* Sleep up to milliseconds, returning early when *running becomes 0.
 * Used by reconnect backoff so stop()/join is not blocked for the full ivl. */
void mb_msleep_while (volatile int *running, int milliseconds);

/* Cap initial reconnect_ivl to reconnect_ivl_max (0 max = uncapped). */
int mb_reconnect_cap_ivl (int ivl, int ivl_max);

/* Double reconnect backoff without signed overflow; clamp to ivl_max. */
int mb_reconnect_next_ivl (int current_ivl, int ivl_max);

#endif
