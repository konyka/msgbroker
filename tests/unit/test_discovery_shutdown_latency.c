/*  test_discovery_shutdown_latency.c — TDD gate for discovery stop wakeup (T-DISC).
 *
 *  Pre-fix: mb_discovery_stop sets running=0 and joins the worker
 *  thread. The worker spends most of its life inside
 *      usleep (interval_ms * 1000)
 *  at the end of each loop iteration, so mb_discovery_term blocks
 *  for up to interval_ms. The TDD gate below starts the worker,
 *  waits long enough for it to enter the usleep, then asserts
 *  mb_discovery_term returns in well under interval_ms — i.e. the
 *  worker must wake immediately on stop.
 *
 *  Mirrors tests/unit/test_gossip_shutdown_latency.c (T-CAND10).
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "../../src/distributed/discovery.h"
#include "../../src/pal/clock.h"

int main (void)
{
    struct mb_discovery d;
    struct mb_discovery_config cfg;
    uint64_t t0, elapsed;

    cfg.local_node_id = 1;
    snprintf (cfg.local_addr, sizeof (cfg.local_addr), "127.0.0.1:1");
    cfg.multicast_group[0] = '\0';
    cfg.port = 0;
    cfg.interval_ms = 2000;

    mb_discovery_init (&d, &cfg);
    (void) mb_discovery_start (&d);

    /* Give the worker time to clear the 100 ms select() phase and
     * enter the multi-second usleep. Without this, stop can race the
     * thread startup and pass spuriously on the unpatched code. */
    struct timespec ts = {0, 250 * 1000 * 1000};
    nanosleep (&ts, NULL);

    t0 = mb_clock_ms ();
    mb_discovery_stop (&d);
    elapsed = mb_clock_ms () - t0;

    /* The worker must wake up well inside the interval. Allow generous
     * slack for CI noise but a stop blocked for the full 2s is a fail. */
    assert (elapsed < 200);

    mb_discovery_term (&d);

    printf ("test_discovery_shutdown_latency: PASSED (elapsed=%llu)\n",
        (unsigned long long) elapsed);
    return 0;
}
