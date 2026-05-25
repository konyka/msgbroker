#include "../../src/aio/threadpool.h"
#include "../../src/pal/atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static mb_atomic_int test_counter;

static void increment_fn (void *arg)
{
    (void) arg;
    mb_atomic_fetch_add (&test_counter, 1);
}

int main (void)
{
    struct mb_threadpool pool;
    int rc = mb_threadpool_init (&pool, 4);
    assert (rc == 0);

    mb_atomic_store (&test_counter, 0);

    struct mb_threadpool_task tasks [100];
    int i;
    for (i = 0; i < 100; i++) {
        mb_queue_item_init (&tasks[i].item);
        tasks[i].fn = increment_fn;
        tasks[i].arg = NULL;
        mb_threadpool_submit (&pool, &tasks[i]);
    }

    mb_threadpool_wait (&pool);

    assert (mb_atomic_load (&test_counter) == 100);
    printf ("test_threadpool: PASSED (counter=%d)\n",
        mb_atomic_load (&test_counter));

    mb_threadpool_term (&pool);
    return 0;
}
