#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include "../../src/pal/thread.h"

struct device_args {
    int s1;
    int s2;
};

static void device_thread (void *arg)
{
    struct device_args *a = (struct device_args *) arg;
    (void) mb_device (a->s1, a->s2);
}

int main (void)
{
    int left, right, c, s;
    int rc;
    char buf[64];
    struct mb_thread thr;
    struct device_args args;

    left = mb_socket (AF_MB, MB_PAIR);
    assert (left >= 0);
    right = mb_socket (AF_MB, MB_PAIR);
    assert (right >= 0);

    rc = mb_bind (left, "inproc://device_left");
    assert (rc >= 0);
    rc = mb_bind (right, "inproc://device_right");
    assert (rc >= 0);

    args.s1 = left;
    args.s2 = right;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, device_thread, &args);
    assert (rc == 0);

    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);
    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_connect (c, "inproc://device_left");
    assert (rc >= 0);
    rc = mb_connect (s, "inproc://device_right");
    assert (rc >= 0);

    usleep (50000);

    /* Idle device must keep running so a later message still forwards. */
    usleep (20000);

    rc = mb_send (c, "HELLO", 5, 0);
    assert (rc == 5);
    rc = mb_recv (s, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s, "WORLD", 5, 0);
    assert (rc == 5);
    rc = mb_recv (c, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    mb_close (c);
    mb_close (s);
    mb_close (left);
    mb_close (right);
    mb_thread_join (&thr);
    mb_thread_term (&thr);

    printf ("test_device: PASSED\n");
    return 0;
}
