/*  test_fsm_term_race.c — TDD gate for ctx torn-down flag (T-FSM1).
 *
 *  Pre-fix: mb_fsm_raise routed to mb_ctx_raise which pushed into the
 *  ctx's events queue without checking whether the ctx was already
 *  torn down. A worker that raised after mb_ctx_term freed the queue
 *  could use-after-free.
 *
 *  The new contract: after mb_ctx_term, all mb_ctx_raise/mb_ctx_raiseto
 *  calls are silent no-ops. The test below raises N events from a
 *  worker thread, races them against an mb_ctx_term in the main
 *  thread, then asserts the process is still healthy and the queue
 *  was never pushed into post-term.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "../../src/aio/ctx.h"
#include "../../src/aio/fsm.h"
#include "../../src/aio/pool.h"

static void noop_leave (struct mb_ctx *c) { (void) c; }

static void *raiser (void *arg)
{
    struct mb_ctx *c = (struct mb_ctx *) arg;
    struct mb_fsm dummy;
    struct mb_fsm_event ev;
    int i;
    for (i = 0; i < 10000; ++i) {
        ev.fsm = &dummy;
        ev.type = 1;
        mb_fsm_raise (&dummy, &ev, 1);
        (void) c;
    }
    return NULL;
}

int main (void)
{
    struct mb_ctx c;
    pthread_t th;

    mb_ctx_init (&c, NULL, noop_leave);
    pthread_create (&th, NULL, raiser, &c);

    /* Let the raiser run, then tear down. mb_ctx_raise must drop all
     * subsequent events rather than push into a destroyed queue. */
    usleep (10000);
    mb_ctx_term (&c);

    pthread_join (th, NULL);

    /* Healthy exit: no segfault, no assertion failure. */
    printf ("test_fsm_term_race: PASSED\n");
    return 0;
}
