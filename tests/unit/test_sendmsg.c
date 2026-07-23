#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s1, s2, rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://iovec");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://iovec");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    memset (buf, 0, sizeof (buf));
    rc = mb_recv (s1, buf, 3, 0);
    assert (rc == 5);
    assert (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L');

    rc = mb_send (s2, "WORLD", 5, 0);
    assert (rc == 5);

    memset (buf, 0, sizeof (buf));
    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    {
        struct mb_iovec iov[3];
        struct mb_msghdr hdr;
        char part1[] = "AB";
        char part2[] = "CD";
        char part3[] = "EF";

        memset (&hdr, 0, sizeof (hdr));
        iov[0].iov_base = part1; iov[0].iov_len = 2;
        iov[1].iov_base = part2; iov[1].iov_len = 2;
        iov[2].iov_base = part3; iov[2].iov_len = 2;
        hdr.msg_iov = iov;
        hdr.msg_iovlen = 3;

        rc = mb_sendmsg (s2, &hdr, 0);
        assert (rc == 6);
    }

    {
        struct mb_iovec iov[2];
        struct mb_msghdr hdr;
        char r1[4], r2[4];

        memset (&hdr, 0, sizeof (hdr));
        iov[0].iov_base = r1; iov[0].iov_len = 4;
        iov[1].iov_base = r2; iov[1].iov_len = 4;
        hdr.msg_iov = iov;
        hdr.msg_iovlen = 2;

        rc = mb_recvmsg (s1, &hdr, 0);
        assert (rc == 6);
        assert (memcmp (r1, "ABCD", 4) == 0);
        assert (memcmp (r2, "EF", 2) == 0);
    }

    mb_close (s2);
    mb_close (s1);

    {
        int s;
        char b = 'x';
        struct mb_iovec iov[2];
        struct mb_msghdr hdr;

        s = mb_socket (AF_MB, MB_PAIR);
        assert (s >= 0);

        memset (&hdr, 0, sizeof (hdr));
        iov[0].iov_base = &b;
        iov[0].iov_len = (size_t) -1 / 2 + 1;
        iov[1].iov_base = &b;
        iov[1].iov_len = (size_t) -1 / 2 + 1;
        hdr.msg_iov = iov;
        hdr.msg_iovlen = 2;

        rc = mb_sendmsg (s, &hdr, MB_DONTWAIT);
        assert (rc == -1);
        assert (mb_errno () == EMSGSIZE);

        mb_close (s);
        printf ("  sendmsg_iov_overflow: OK\n");
    }

    {
        int s;
        struct mb_iovec iov;
        struct mb_msghdr hdr;

        s = mb_socket (AF_MB, MB_PAIR);
        assert (s >= 0);

        memset (&hdr, 0, sizeof (hdr));
        iov.iov_base = NULL;
        iov.iov_len = 1;
        hdr.msg_iov = &iov;
        hdr.msg_iovlen = 1;

        rc = mb_sendmsg (s, &hdr, MB_DONTWAIT);
        assert (rc == -1);
        assert (mb_errno () == EFAULT);

        mb_close (s);
        printf ("  sendmsg_null_iov_base: OK\n");
    }

    printf ("test_sendmsg: PASSED\n");
    return 0;
}
