#include "../../src/aio/evloop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

static void test_evloop_init_term (void)
{
    struct mb_evloop evloop;
    int rc = mb_evloop_init (&evloop);
    assert (rc == 0);

    printf ("  backend: %s\n", mb_evloop_backend_name (&evloop));

    mb_evloop_term (&evloop);
    printf ("  test_evloop_init_term: PASSED\n");
}

static int g_callback_count;
static int g_callback_events;

static void test_callback (void *data, int events)
{
    (void) data;
    g_callback_count++;
    g_callback_events = events;
}

static void test_evloop_add_poll (void)
{
    struct mb_evloop evloop;
    int rc = mb_evloop_init (&evloop);
    assert (rc == 0);

    int pipefd[2];
    rc = pipe (pipefd);
    assert (rc == 0);

    struct mb_evloop_cb cb;
    cb.on_event = test_callback;
    cb.data = NULL;

    rc = mb_evloop_add (&evloop, pipefd[0], MB_EVLOOP_IN, &cb);
    assert (rc == 0);

    g_callback_count = 0;
    g_callback_events = 0;

    write (pipefd[1], "X", 1);

    int n = mb_evloop_poll (&evloop, 100);
    assert (n >= 1);
    assert (g_callback_count == 1);
    assert (g_callback_events & MB_EVLOOP_IN);

    mb_evloop_remove (&evloop, pipefd[0]);
    close (pipefd[0]);
    close (pipefd[1]);
    mb_evloop_term (&evloop);

    printf ("  test_evloop_add_poll: PASSED\n");
}

static void test_evloop_timeout (void)
{
    struct mb_evloop evloop;
    int rc = mb_evloop_init (&evloop);
    assert (rc == 0);

    int n = mb_evloop_poll (&evloop, 0);
    assert (n == 0);

    mb_evloop_term (&evloop);
    printf ("  test_evloop_timeout: PASSED\n");
}

static void test_evloop_modify (void)
{
    struct mb_evloop evloop;
    int rc = mb_evloop_init (&evloop);
    assert (rc == 0);

    int pipefd[2];
    rc = pipe (pipefd);
    assert (rc == 0);

    struct mb_evloop_cb cb;
    cb.on_event = test_callback;
    cb.data = NULL;

    rc = mb_evloop_add (&evloop, pipefd[0], MB_EVLOOP_IN, &cb);
    assert (rc == 0);

    rc = mb_evloop_modify (&evloop, pipefd[0],
        MB_EVLOOP_IN | MB_EVLOOP_OUT, &cb);
    assert (rc == 0);

    mb_evloop_remove (&evloop, pipefd[0]);
    close (pipefd[0]);
    close (pipefd[1]);
    mb_evloop_term (&evloop);

    printf ("  test_evloop_modify: PASSED\n");
}

int main (void)
{
    printf ("evloop tests:\n");
    test_evloop_init_term ();
    test_evloop_add_poll ();
    test_evloop_timeout ();
    test_evloop_modify ();
    printf ("All evloop tests passed.\n");
    return 0;
}
