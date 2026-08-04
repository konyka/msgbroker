/*
    msgbroker -- High-performance messaging library in pure C.

    Copyright 2024 msgbroker contributors. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

/*  T-PRIO: per-priority send queue on PUB socket.

    Validates:
      - Sendmsg with MB_PRIO_HIGH cmsg causes high-priority messages to be
        delivered before normal-priority messages that were enqueued first.
      - MB_PRIO_LOW / MB_PRIO_NORMAL / MB_PRIO_HIGH / MB_PRIO_CRITICAL are
        accepted (8-level field, all in valid range).
      - Default (no cmsg) is normal priority.
      - MB_STAT_CURRENT_SND_PRIORITY returns the highest non-empty bucket.
      - Out-of-range priorities are rejected with EINVAL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

/*  T-PRIO exposes the priority cmsg layout via PROTO_SP / SP_HDR plus a
 *  one-byte value. The encoding lives in pub.c; we send it via mb_sendmsg
 *  with a cmsghdr containing the priority int. */
#define MB_PRIO_PROTO        PROTO_SP
#define MB_PRIO_HDR_TYPE     SP_HDR
#define MB_PRIO_LOW          1
#define MB_PRIO_NORMAL       4
#define MB_PRIO_HIGH         6
#define MB_PRIO_CRITICAL     7

static uint64_t get_stat (int s, int stat)
{
    uint64_t v = 0;
    size_t sz = sizeof (v);
    int rc = mb_getsockopt (s, MB_SOL_SOCKET, stat, &v, &sz);
    assert (rc == 0);
    return v;
}

static int send_with_prio (int s, const char *payload, size_t plen, int prio)
{
    struct mb_iovec iov;
    struct mb_msghdr hdr;
    char cbuf[MB_CMSG_SPACE (sizeof (int))];
    struct mb_cmsghdr *cmsg;
    int rc;

    iov.iov_base = (void *) payload;
    iov.iov_len = plen;

    memset (cbuf, 0, sizeof (cbuf));
    cmsg = (struct mb_cmsghdr *) cbuf;
    cmsg->cmsg_level = MB_PRIO_PROTO;
    cmsg->cmsg_type = MB_PRIO_HDR_TYPE;
    cmsg->cmsg_len = MB_CMSG_LEN (sizeof (int));
    memcpy (MB_CMSG_DATA (cmsg), &prio, sizeof (int));

    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = cbuf;
    hdr.msg_controllen = MB_CMSG_SPACE (sizeof (int));

    /* DONTWAIT so -EAGAIN propagates. T-PRIO PUB retains and surfaces
     * -EAGAIN; both rc==1 and (rc<0 && errno==EAGAIN) are valid here. */
    rc = mb_sendmsg (s, &hdr, MB_DONTWAIT);
    if (rc < 0 && mb_errno () == EAGAIN)
        return 0;
    return rc;
}

/*  Core scenario per the task spec: send 3 high then 3 normal; receiver
 *  asserts high comes first. We force the PUB to defer messages by setting
 *  the SUB's RCVBUF low enough that the inproc msgqueue rejects pushes,
 *  causing mb_pipe_send to return -EAGAIN. The PUB then stashes each
 *  message into its per-priority bucket. Draining happens on the next
 *  mb_send; the SUB's queue then reorders in priority order. */
static void test_priority_ordering (void)
{
    int pub, sub;
    int rc;
    int i;
    char buf[16];
    int rcvbuf;

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);

    /* Tight RCVBUF forces the SUB's msgqueue to reject pushes after the
     * first message lands. */
    rcvbuf = 1;
    rc = mb_setsockopt (sub, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf,
        sizeof (rcvbuf));
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://prio_order");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://prio_order");
    assert (rc >= 0);
    usleep (50000);

    /* Send 3 high first, then 3 normal. The PUB sees -EAGAIN on the
     * second-and-later sends (SUB's 1-byte queue is full) and stashes
     * them in the high / normal buckets respectively. */
    for (i = 0; i < 3; i++) {
        rc = send_with_prio (pub, "H", 1, MB_PRIO_HIGH);
        assert (rc >= 0);
    }
    for (i = 0; i < 3; i++) {
        rc = send_with_prio (pub, "N", 1, MB_PRIO_NORMAL);
        assert (rc >= 0);
    }

    /* Statistic reports the highest non-empty bucket (high = 6). */
    {
        uint64_t st = get_stat (pub, MB_STAT_CURRENT_SND_PRIORITY);
        assert (st == MB_PRIO_HIGH);
    }

    /* Widen the SUB's buffer so the next send can drain the high bucket
     * first (priority order) and then the normal bucket. */
    rcvbuf = 1024 * 1024;
    rc = mb_setsockopt (sub, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf,
        sizeof (rcvbuf));
    assert (rc == 0);

    /* Trigger a drain with a fresh send. mb_pub_send must flush the
     * highest non-empty bucket (high) before handling the new payload. */
    rc = send_with_prio (pub, "X", 1, MB_PRIO_NORMAL);
    assert (rc >= 0);
    usleep (50000);

    /* Drain the receiver: 3 high, then 3 normal from buckets, then 'X'
     * from the trigger. */
    for (i = 0; i < 3; i++) {
        memset (buf, 0, sizeof (buf));
        rc = mb_recv (sub, buf, sizeof (buf), 0);
        assert (rc >= 0);
        assert (buf[0] == 'H');
    }
    for (i = 0; i < 3; i++) {
        memset (buf, 0, sizeof (buf));
        rc = mb_recv (sub, buf, sizeof (buf), 0);
        assert (rc >= 0);
        assert (buf[0] == 'N');
    }
    memset (buf, 0, sizeof (buf));
    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc >= 0);
    assert (buf[0] == 'X');

    mb_close (sub);
    mb_close (pub);

    printf ("  test_priority_ordering: PASSED\n");
}

static void test_default_is_normal (void)
{
    int pub, sub;
    int rc;

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://prio_default");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://prio_default");
    assert (rc >= 0);
    usleep (50000);

    /* Default (no cmsg) priority is normal. Tight RCVBUF forces defer. */
    {
        int rcvbuf = 1;
        rc = mb_setsockopt (sub, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf,
            sizeof (rcvbuf));
        assert (rc == 0);
    }

    rc = mb_send (pub, "Z", 1, MB_DONTWAIT);
    assert (rc >= 0 || mb_errno () == EAGAIN);
    /* SUB queue is now full (RCVBUF=1, body=1). A second send is
     * rejected and stashed in the normal bucket. */
    rc = mb_send (pub, "Z", 1, MB_DONTWAIT);
    assert (rc >= 0 || mb_errno () == EAGAIN);

    {
        uint64_t st = get_stat (pub, MB_STAT_CURRENT_SND_PRIORITY);
        assert (st == MB_PRIO_NORMAL);
    }

    mb_close (sub);
    mb_close (pub);

    printf ("  test_default_is_normal: PASSED\n");
}

static void test_critical_drains_first (void)
{
    int pub, sub;
    int rc;
    int i;
    char buf[8];
    int rcvbuf;
    const char *expected = "LCHNX";

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);
    rcvbuf = 1;
    rc = mb_setsockopt (sub, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf,
        sizeof (rcvbuf));
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://prio_crit");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://prio_crit");
    assert (rc >= 0);
    usleep (50000);

    /* Send in non-priority order: L, N, H, C. With RCVBUF=1 the first
     * send fits the SUB queue (L stays there), the rest are rejected
     * and stashed in their priority buckets on the PUB side. */
    rc = send_with_prio (pub, "L", 1, MB_PRIO_LOW);       assert (rc >= 0);
    rc = send_with_prio (pub, "N", 1, MB_PRIO_NORMAL);    assert (rc >= 0);
    rc = send_with_prio (pub, "H", 1, MB_PRIO_HIGH);      assert (rc >= 0);
    rc = send_with_prio (pub, "C", 1, MB_PRIO_CRITICAL);  assert (rc >= 0);

    {
        uint64_t st = get_stat (pub, MB_STAT_CURRENT_SND_PRIORITY);
        assert (st == MB_PRIO_CRITICAL);
    }

    /* Widen and trigger drain. */
    rcvbuf = 1024 * 1024;
    rc = mb_setsockopt (sub, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf,
        sizeof (rcvbuf));
    assert (rc == 0);
    rc = send_with_prio (pub, "X", 1, MB_PRIO_NORMAL);
    assert (rc >= 0);
    usleep (50000);

    /* Order on the receiver: L (was already in queue), then the
     * priority-ordered backlog C, H, N, then the trigger X. */
    for (i = 0; i < 5; i++) {
        memset (buf, 0, sizeof (buf));
        rc = mb_recv (sub, buf, sizeof (buf), 0);
        assert (rc >= 0);
        assert (buf[0] == expected[i]);
    }

    mb_close (sub);
    mb_close (pub);

    printf ("  test_critical_drains_first: PASSED\n");
}

static void test_prio_out_of_range_rejected (void)
{
    int pub, rc;
    char cbuf[MB_CMSG_SPACE (sizeof (int))];
    struct mb_cmsghdr *cmsg;
    struct mb_iovec iov;
    struct mb_msghdr hdr;
    int bogus = 99;

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);

    iov.iov_base = (void *) "x"; iov.iov_len = 1;
    memset (cbuf, 0, sizeof (cbuf));
    cmsg = (struct mb_cmsghdr *) cbuf;
    cmsg->cmsg_level = MB_PRIO_PROTO;
    cmsg->cmsg_type = MB_PRIO_HDR_TYPE;
    cmsg->cmsg_len = MB_CMSG_LEN (sizeof (int));
    memcpy (MB_CMSG_DATA (cmsg), &bogus, sizeof (int));
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = cbuf;
    hdr.msg_controllen = MB_CMSG_SPACE (sizeof (int));

    rc = mb_sendmsg (pub, &hdr, 0);
    assert (rc < 0);
    assert (mb_errno () == EINVAL);

    mb_close (pub);

    printf ("  test_prio_out_of_range_rejected: PASSED\n");
}

int main (void)
{
    test_priority_ordering ();
    test_default_is_normal ();
    test_critical_drains_first ();
    test_prio_out_of_range_rejected ();
    printf ("test_pubsub_priority: ALL PASSED\n");
    return 0;
}