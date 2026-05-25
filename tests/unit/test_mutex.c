#include "../../src/pal/mutex.h"
#include "../../src/pal/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int shared_counter = 0;
static struct mb_mutex test_mutex;

static void mutex_thread_fn (void *arg)
{
    int iterations = *(int *) arg;
    int i;
    for (i = 0; i < iterations; i++) {
        mb_mutex_lock (&test_mutex);
        shared_counter++;
        mb_mutex_unlock (&test_mutex);
    }
}

int main (void)
{
    struct mb_thread t1, t2;
    int iterations = 100000;

    mb_mutex_init (&test_mutex);
    shared_counter = 0;

    mb_thread_init (&t1);
    mb_thread_init (&t2);

    int rc1 = mb_thread_start (&t1, mutex_thread_fn, &iterations);
    int rc2 = mb_thread_start (&t2, mutex_thread_fn, &iterations);
    assert (rc1 == 0);
    assert (rc2 == 0);
    (void) rc1;
    (void) rc2;

    mb_thread_join (&t1);
    mb_thread_join (&t2);

    assert (shared_counter == iterations * 2);
    printf ("test_mutex: PASSED (counter=%d expected=%d)\n",
        shared_counter, iterations * 2);

    mb_thread_term (&t1);
    mb_thread_term (&t2);
    mb_mutex_term (&test_mutex);
    return 0;
}
