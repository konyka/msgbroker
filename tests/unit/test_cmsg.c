#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_cmsg_nxthdr_empty (void)
{
    struct mb_msghdr hdr;
    struct mb_cmsghdr *cmsg;

    memset (&hdr, 0, sizeof (hdr));

    cmsg = MB_CMSG_FIRSTHDR (&hdr);
    assert (cmsg == NULL);

    printf ("  cmsg_nxthdr_empty: OK\n");
}

static void test_cmsg_nxthdr_single (void)
{
    struct mb_msghdr hdr;
    struct mb_cmsghdr *cmsg;
    char cbuf[128];

    memset (cbuf, 0, sizeof (cbuf));
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_control = cbuf;
    hdr.msg_controllen = MB_CMSG_SPACE (4);

    cmsg = MB_CMSG_FIRSTHDR (&hdr);
    assert (cmsg != NULL);

    cmsg->cmsg_len = MB_CMSG_LEN (4);
    cmsg->cmsg_level = PROTO_SP;
    cmsg->cmsg_type = SP_HDR;
    memset (MB_CMSG_DATA (cmsg), 0xAB, 4);

    cmsg = MB_CMSG_NXTHDR (&hdr, cmsg);
    assert (cmsg == NULL);

    printf ("  cmsg_nxthdr_single: OK\n");
}

static void test_cmsg_nxthdr_multiple (void)
{
    struct mb_msghdr hdr;
    struct mb_cmsghdr *cmsg;
    char cbuf[256];
    int count;

    memset (cbuf, 0, sizeof (cbuf));
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_control = cbuf;
    hdr.msg_controllen = MB_CMSG_SPACE (4) + MB_CMSG_SPACE (8);

    cmsg = MB_CMSG_FIRSTHDR (&hdr);
    assert (cmsg != NULL);
    cmsg->cmsg_len = MB_CMSG_LEN (4);
    cmsg->cmsg_level = PROTO_SP;
    cmsg->cmsg_type = SP_HDR;

    cmsg = MB_CMSG_NXTHDR (&hdr, cmsg);
    assert (cmsg != NULL);
    cmsg->cmsg_len = MB_CMSG_LEN (8);
    cmsg->cmsg_level = PROTO_SP;
    cmsg->cmsg_type = SP_HDR;

    cmsg = MB_CMSG_NXTHDR (&hdr, cmsg);
    assert (cmsg == NULL);

    count = 0;
    for (cmsg = MB_CMSG_FIRSTHDR (&hdr); cmsg;
         cmsg = MB_CMSG_NXTHDR (&hdr, cmsg))
        ++count;
    assert (count == 2);

    printf ("  cmsg_nxthdr_multiple: OK\n");
}

static void test_cmsg_macros (void)
{
    assert (MB_CMSG_LEN (0) >= sizeof (struct mb_cmsghdr));
    assert (MB_CMSG_SPACE (4) >= MB_CMSG_LEN (4));
    assert (MB_CMSG_SPACE (4) > MB_CMSG_LEN (4) || MB_CMSG_SPACE (4) == MB_CMSG_LEN (4));

    {
        char cbuf[128];
        struct mb_cmsghdr *cmsg;
        memset (cbuf, 0xCC, sizeof (cbuf));

        cmsg = (struct mb_cmsghdr *) cbuf;
        cmsg->cmsg_len = MB_CMSG_LEN (4);
        cmsg->cmsg_level = 1;
        cmsg->cmsg_type = 2;
        memcpy (MB_CMSG_DATA (cmsg), "TEST", 4);

        assert (memcmp (MB_CMSG_DATA (cmsg), "TEST", 4) == 0);
    }

    printf ("  cmsg_macros: OK\n");
}

static void test_cmsg_recvmsg_no_control (void)
{
    int s1, s2, rc;
    struct mb_iovec iov;
    struct mb_msghdr hdr;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://cmsg");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://cmsg");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    memset (&hdr, 0, sizeof (hdr));
    iov.iov_base = buf;
    iov.iov_len = sizeof (buf);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;

    rc = mb_recvmsg (s1, &hdr, 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    mb_close (s2);
    mb_close (s1);

    printf ("  cmsg_recvmsg_no_control: OK\n");
}

static void test_sendmsg_short_sphdr_cmsg (void)
{
    int s, rc;
    char b = 'x';
    char cbuf[128];
    struct mb_iovec iov;
    struct mb_msghdr hdr;
    struct mb_cmsghdr *cmsg;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    memset (cbuf, 0, sizeof (cbuf));
    cmsg = (struct mb_cmsghdr *) cbuf;
    cmsg->cmsg_len = MB_CMSG_LEN (0) - 1;
    cmsg->cmsg_level = PROTO_SP;
    cmsg->cmsg_type = SP_HDR;

    memset (&hdr, 0, sizeof (hdr));
    iov.iov_base = &b;
    iov.iov_len = 1;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = cbuf;
    hdr.msg_controllen = sizeof (cbuf);

    rc = mb_sendmsg (s, &hdr, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    mb_close (s);
    printf ("  sendmsg_short_sphdr_cmsg: OK\n");
}

int main (void)
{
    printf ("test_cmsg:\n");
    test_cmsg_nxthdr_empty ();
    test_cmsg_nxthdr_single ();
    test_cmsg_nxthdr_multiple ();
    test_cmsg_macros ();
    test_cmsg_recvmsg_no_control ();
    test_sendmsg_short_sphdr_cmsg ();
    printf ("test_cmsg: PASSED\n");
    return 0;
}
