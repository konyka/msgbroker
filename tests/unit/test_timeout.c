#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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
    assert (mb_errno () == ETIMEDOUT);

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
    assert (mb_errno () == ETIMEDOUT);

    mb_close (s);

    printf ("  timeout_send: OK\n");
}

static void test_timeout_sendmsg_recvmsg (void)
{
    int s, rc;
    int val;
    char payload[] = "X";
    char rbuf[16];
    struct mb_iovec iov;
    struct mb_msghdr hdr;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    memset (&hdr, 0, sizeof (hdr));
    iov.iov_base = rbuf;
    iov.iov_len = sizeof (rbuf);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    rc = mb_recvmsg (s, &hdr, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    memset (&hdr, 0, sizeof (hdr));
    iov.iov_base = payload;
    iov.iov_len = 1;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    rc = mb_sendmsg (s, &hdr, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    val = 10;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, sizeof (val));
    assert (rc == 0);
    rc = mb_sendmsg (s, &hdr, 0);
    assert (rc == -1);
    assert (mb_errno () == ETIMEDOUT);

    val = 10;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, sizeof (val));
    assert (rc == 0);
    memset (&hdr, 0, sizeof (hdr));
    iov.iov_base = rbuf;
    iov.iov_len = sizeof (rbuf);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    rc = mb_recvmsg (s, &hdr, 0);
    assert (rc == -1);
    assert (mb_errno () == ETIMEDOUT);

    mb_close (s);

    printf ("  timeout_sendmsg_recvmsg: OK\n");
}

static void test_send_oom_large_body (void)
{
    int s, rc;
    int unlimited = -1;
    char b = 'x';
    /* Absurd length: malloc fails on common platforms; must return ENOMEM.
     * Disable MB_RCVMAXSIZE so the API size gate does not return EMSGSIZE. */
    size_t huge = (size_t) -1 / 4;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVMAXSIZE, &unlimited,
        sizeof (unlimited));
    assert (rc == 0);

    rc = mb_send (s, &b, huge, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == ENOMEM);

    mb_close (s);
    printf ("  send_oom_large_body: OK\n");
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
    test_timeout_sendmsg_recvmsg ();
    test_send_oom_large_body ();
    test_version ();
    printf ("test_timeout: PASSED\n");
    return 0;
}
