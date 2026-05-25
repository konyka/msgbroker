#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_dontwait_eagain (void)
{
    int s, rc;
    char buf[16];

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_recv (s, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    rc = mb_send (s, "X", 1, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    mb_close (s);

    printf ("  dontwait_eagain: OK\n");
}

static void test_timeout_recv (void)
{
    int s, rc;
    int val;
    char buf[16];

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 10;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, sizeof (val));
    assert (rc == 0);

    rc = mb_recv (s, buf, sizeof (buf), 0);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    mb_close (s);

    printf ("  timeout_recv: OK\n");
}

static void test_timeout_send (void)
{
    int s, rc;
    int val;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 10;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, sizeof (val));
    assert (rc == 0);

    rc = mb_send (s, "X", 1, 0);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    mb_close (s);

    printf ("  timeout_send: OK\n");
}

static void test_version (void)
{
    assert (mb_version_major () == MB_VERSION_MAJOR);
    assert (mb_version_minor () == MB_VERSION_MINOR);
    assert (mb_version_patch () == MB_VERSION_PATCH);
    assert (mb_version_string () != NULL);
    assert (mb_version_string ()[0] >= '0');
    printf ("  version: OK (v%s)\n", mb_version_string ());
}

int main (void)
{
    printf ("test_timeout:\n");
    test_dontwait_eagain ();
    test_timeout_recv ();
    test_timeout_send ();
    test_version ();
    printf ("test_timeout: PASSED\n");
    return 0;
}
