#include "../../src/pal/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int thread_ran = 0;

static void simple_thread_fn (void *arg)
{
    int *val = (int *) arg;
    *val = 42;
    thread_ran = 1;
}

int main (void)
{
    struct mb_thread t;
    int val = 0;

    mb_thread_init (&t);
    int rc = mb_thread_start (&t, simple_thread_fn, &val);
    assert (rc == 0);
    (void) rc;

    mb_thread_join (&t);

    assert (val == 42);
    assert (thread_ran == 1);
    printf ("test_thread: PASSED\n");

    mb_thread_term (&t);
    return 0;
}
