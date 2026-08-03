/*  test_gossip_shutdown_latency.c — TDD gate for gossip stop wakeup (T-CAND10).
 *
 *  Pre-fix: mb_gossip_stop sets running=0 and joins the worker
 *  thread. The worker only checks running at the next usleep boundary,
 *  so mb_gossip_term blocks for up to interval_ms. The TDD gate
 *  below uses a 2-second interval and asserts mb_gossip_term returns
 *  in well under that — i.e. the worker must wake immediately on stop.
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "../../src/distributed/gossip.h"
#include "../../src/pal/clock.h"

int main (void)
{
    struct mb_gossip g;
    struct mb_gossip_config cfg;
    uint64_t t0, elapsed;

    cfg.local_node_id = 1;
    snprintf (cfg.local_addr, sizeof (cfg.local_addr), "127.0.0.1:1");
    cfg.interval_ms = 2000;
    cfg.suspect_timeout_ms = 0;
    cfg.dead_timeout_ms = 0;

    mb_gossip_init (&g, &cfg);
    (void) mb_gossip_start (&g);

    t0 = mb_clock_ms ();
    mb_gossip_stop (&g);
    elapsed = mb_clock_ms () - t0;

    /* The worker must wake up well inside the interval. Allow generous
     * slack for CI noise but a stop blocked for the full 2s is a fail. */
    assert (elapsed < 200);

    mb_gossip_term (&g);

    printf ("test_gossip_shutdown_latency: PASSED (elapsed=%llu)\n",
        (unsigned long long) elapsed);
    return 0;
}
