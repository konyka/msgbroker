/*  test_mpsc_chaos.c — TDD gate for the C11-atomic MPSC migration (T-MPSC2).
 *
 *  This test is written BEFORE the implementation change. It exercises
 *  the MPSC lock-free queue under high contention: 4 producer threads
 *  push 2,500,000 items each (10,000,000 total) while a single consumer
 *  drains the queue, and we assert that:
 *
 *    1. The total number of items received equals the total pushed.
 *    2. Every (producer_id, slot_id) pair appears exactly once.
 *
 *  Each item carries a 64-bit identity: (producer_id << 32) | slot_id,
 *  so duplicates and gaps are easy to detect.
 *
 *  This file lives in tests/fuzz/ per the T-MPSC2 task plan but is
 *  compiled as plain C, linked against msgbroker_static, and registered
 *  with ctest so it runs on every build.
 */

#include "../../src/utils/queue.h"
#include "../../src/pal/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define NUM_PRODUCERS     4
#define ITEMS_PER_PRODUCER 2500000  /* 4 * 2.5M = 10M total */
#define TOTAL_ITEMS       ((uint64_t) NUM_PRODUCERS * ITEMS_PER_PRODUCER)

/*  Each pushed item carries a 64-bit id so the consumer can verify
 *  no loss and no duplicate.  The user data lives at a fixed offset
 *  from the embedded mb_mpsc_queue_item, matching how the rest of
 *  msgbroker uses container_of. */
struct mpsc_test_item {
    uint64_t id;
    struct mb_mpsc_queue_item node;
};

/*  Build the identity for producer p, slot s. */
static inline uint64_t make_id (unsigned p, uint32_t s)
{
    return ((uint64_t) p << 32) | (uint64_t) s;
}

struct producer_arg {
    unsigned                id;
    struct mpsc_test_item  *items;
};

static void producer_fn (void *arg)
{
    struct producer_arg *pa = (struct producer_arg *) arg;
    /*  The queue is created on the main thread and the consumer only
     *  starts once every producer has been launched, so the queue is
     *  safe to push to from here without further coordination. */
    extern struct mb_mpsc_queue *get_test_queue (void);  /* see below */
    struct mb_mpsc_queue *q = get_test_queue ();
    for (uint32_t i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        pa->items[i].id = make_id (pa->id, i);
        mb_mpsc_queue_push (q, &pa->items[i].node);
    }
}

/*  A single global queue for the test. Tests don't run in parallel
 *  against each other, so a file-scope global is fine. */
static struct mb_mpsc_queue g_queue;

struct mb_mpsc_queue *get_test_queue (void) { return &g_queue; }

int main (void)
{
    /*  Pre-allocate all 10M items in one shot — 80 MB on 64-bit.
     *  Allocating inside the producers would be far too slow. */
    struct mpsc_test_item *items = (struct mpsc_test_item *)
        malloc ((size_t) TOTAL_ITEMS * sizeof (*items));
    assert (items != NULL);
    memset (items, 0, (size_t) TOTAL_ITEMS * sizeof (*items));

    /*  seen[p * ITEMS_PER_PRODUCER + s] is 1 once the consumer has
     *  observed (p, s).  10M bytes ≈ 10 MB. */
    unsigned char *seen = (unsigned char *)
        calloc ((size_t) TOTAL_ITEMS, 1);
    assert (seen != NULL);

    mb_mpsc_queue_init (&g_queue);

    /*  Launch producers. We start them FIRST, then race to drain.
     *  The whole point is to stress concurrent producers — having
     *  them run before the consumer starts amplifies contention
     *  on producer_tail. */
    struct mb_thread   threads[NUM_PRODUCERS];
    struct producer_arg args[NUM_PRODUCERS];
    for (unsigned p = 0; p < NUM_PRODUCERS; ++p) {
        args[p].id = p;
        args[p].items = items + (size_t) p * ITEMS_PER_PRODUCER;
        int rc = mb_thread_start (&threads[p], producer_fn, &args[p]);
        assert (rc == 0);
    }

    /*  Consumer: drain until we've seen TOTAL_ITEMS unique ids.
     *  MPSC pop is allowed to return NULL transiently while producers
     *  are still active; we just keep trying. */
    uint64_t received = 0;
    uint64_t duplicate = 0;
    while (received < TOTAL_ITEMS) {
        struct mb_mpsc_queue_item *node = mb_mpsc_queue_pop (&g_queue);
        if (node == NULL)
            continue;
        struct mpsc_test_item *it = ((struct mpsc_test_item *) (
            (char *) node - offsetof (struct mpsc_test_item, node)));
        uint64_t id = it->id;
        unsigned p = (unsigned) (id >> 32);
        uint32_t s = (uint32_t) id;
        assert (p < NUM_PRODUCERS);
        assert (s < ITEMS_PER_PRODUCER);
        size_t idx = (size_t) p * ITEMS_PER_PRODUCER + s;
        if (seen[idx])
            ++duplicate;
        else
            seen[idx] = 1;
        ++received;
    }

    /*  Wait for every producer to actually finish so the queue can
     *  be torn down safely. */
    for (unsigned p = 0; p < NUM_PRODUCERS; ++p)
        mb_thread_join (&threads[p]);

    /*  Queue should now be empty.  Calling pop on an empty queue
     *  is the caller's responsibility; we just verify no items
     *  remain in the array. */
    assert (received == TOTAL_ITEMS);
    assert (duplicate == 0);
    for (uint64_t i = 0; i < TOTAL_ITEMS; ++i)
        assert (seen[i] == 1);

    mb_mpsc_queue_term (&g_queue);

    free (seen);
    free (items);

    printf ("test_mpsc_chaos: PASSED (received=%llu, duplicate=%llu)\n",
        (unsigned long long) received, (unsigned long long) duplicate);
    return 0;
}
