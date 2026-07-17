#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_reqrep.h>

/*  REQ->REP: send request, receive reply. */
static void test_reqrep_inproc (void)
{
    int req, rep;
    int rc;
    char buf[64];

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://reqrep1");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://reqrep1");
    assert (rc >= 0);

    /*  REQ sends request. */
    rc = mb_send (req, "REQ1", 4, 0);
    assert (rc == 4);

    /*  REP receives request. */
    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "REQ1", 4) == 0);

    /*  REP sends reply. */
    rc = mb_send (rep, "REP1", 4, 0);
    assert (rc == 4);

    /*  REQ receives reply. */
    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "REP1", 4) == 0);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_reqrep_inproc: PASSED\n");
}

/*  REQ->REP via TCP. */
static void test_reqrep_tcp (void)
{
    int req, rep;
    int rc;
    char buf[64];

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "tcp://127.0.0.1:19877");
    assert (rc >= 0);
    rc = mb_connect (req, "tcp://127.0.0.1:19877");
    assert (rc >= 0);

    usleep (200000);

    rc = mb_send (req, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (rep, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_reqrep_tcp: PASSED\n");
}

/*  REP state machine: cannot send before receiving a request. */
static void test_rep_send_before_recv (void)
{
    int rep;
    int rc;

    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_send (rep, "X", 1, MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EFSM);

    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_rep_send_before_recv: PASSED\n");
}

/*  REQ state machine: cannot recv before send. */
static void test_reqrecv_before_send (void)
{
    int req, rep;
    char buf[64];
    int rc;

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://reqrep_fsm");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://reqrep_fsm");
    assert (rc >= 0);

    /*  Recv before send should return EFSM. */
    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc < 0);
    assert (mb_errno () == EFSM);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_reqrecv_before_send: PASSED\n");
}

/*  REP state machine: cannot recv again before sending a reply. */
static void test_reprecv_before_reply (void)
{
    int req, rep;
    char buf[64];
    int rc;

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://reqrep_rep_fsm");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://reqrep_rep_fsm");
    assert (rc >= 0);

    rc = mb_send (req, "REQ1", 4, 0);
    assert (rc == 4);
    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 4);

    /*  Second recv before reply must not overwrite last_pipe. */
    rc = mb_recv (rep, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EFSM);

    rc = mb_send (rep, "REP1", 4, 0);
    assert (rc == 4);
    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "REP1", 4) == 0);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_reprecv_before_reply: PASSED\n");
}

/*  REQ with multiple REPs must round-robin requests. */
static void test_req_lb_rotate (void)
{
    int req, rep1, rep2;
    int rc;
    int i;
    int got1 = 0, got2 = 0;
    char buf[64];

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep1 = mb_socket (AF_MB, MB_REP);
    assert (rep1 >= 0);
    rep2 = mb_socket (AF_MB, MB_REP);
    assert (rep2 >= 0);

    rc = mb_bind (rep1, "inproc://req_lb1");
    assert (rc >= 0);
    rc = mb_bind (rep2, "inproc://req_lb2");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://req_lb1");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://req_lb2");
    assert (rc >= 0);

    for (i = 0; i < 4; ++i) {
        int which = 0;

        rc = mb_send (req, "Q", 1, 0);
        assert (rc == 1);

        rc = mb_recv (rep1, buf, sizeof (buf), MB_DONTWAIT);
        if (rc == 1) {
            which = 1;
            got1++;
            rc = mb_send (rep1, "A", 1, 0);
            assert (rc == 1);
        } else {
            rc = mb_recv (rep2, buf, sizeof (buf), MB_DONTWAIT);
            assert (rc == 1);
            which = 2;
            got2++;
            rc = mb_send (rep2, "A", 1, 0);
            assert (rc == 1);
        }
        (void) which;

        rc = mb_recv (req, buf, sizeof (buf), 0);
        assert (rc == 1);
        assert (buf[0] == 'A');
    }

    assert (got1 >= 1);
    assert (got2 >= 1);
    assert (got1 + got2 == 4);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep1);
    assert (rc == 0);
    rc = mb_close (rep2);
    assert (rc == 0);

    printf ("  test_req_lb_rotate: PASSED\n");
}

/*  REQ must clear POLLOUT while waiting for a reply (sndfd sync). */
static void test_req_poll_no_pollout_while_waiting (void)
{
    int req, rep;
    int rc;
    char buf[64];
    struct mb_pollfd fds[1];

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://req_poll_out");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://req_poll_out");
    assert (rc >= 0);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = req;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLOUT);

    rc = mb_send (req, "Q", 1, 0);
    assert (rc == 1);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 1);
    rc = mb_send (rep, "A", 1, 0);
    assert (rc == 1);
    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 1);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLOUT);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_req_poll_no_pollout_while_waiting: PASSED\n");
}

/*  REP must advertise POLLOUT only after recv (sndfd sync). */
static void test_rep_poll_pollout_after_recv (void)
{
    int req, rep;
    int rc;
    char buf[64];
    struct mb_pollfd fds[1];

    req = mb_socket (AF_MB, MB_REQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_REP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://rep_poll_out");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://rep_poll_out");
    assert (rc >= 0);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = rep;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    rc = mb_send (req, "Q", 1, 0);
    assert (rc == 1);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 1);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLOUT);

    rc = mb_send (rep, "A", 1, 0);
    assert (rc == 1);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 1);

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_rep_poll_pollout_after_recv: PASSED\n");
}

/*  Raw XREQ/XREP must send and recv (not permanent EAGAIN stubs). */
static void test_xreq_xrep_inproc (void)
{
    int req, rep;
    int rc;
    char buf[64];

    req = mb_socket (AF_MB, MB_XREQ);
    assert (req >= 0);
    rep = mb_socket (AF_MB, MB_XREP);
    assert (rep >= 0);

    rc = mb_bind (rep, "inproc://xreqrep");
    assert (rc >= 0);
    rc = mb_connect (req, "inproc://xreqrep");
    assert (rc >= 0);

    rc = mb_send (req, "Q", 1, 0);
    assert (rc == 1);
    rc = mb_recv (rep, buf, sizeof (buf), 0);
    assert (rc == 1);
    assert (buf[0] == 'Q');

    rc = mb_send (rep, "A", 1, 0);
    assert (rc == 1);
    rc = mb_recv (req, buf, sizeof (buf), 0);
    assert (rc == 1);
    assert (buf[0] == 'A');

    rc = mb_close (req);
    assert (rc == 0);
    rc = mb_close (rep);
    assert (rc == 0);

    printf ("  test_xreq_xrep_inproc: PASSED\n");
}

int main (void)
{
    printf ("REQ/REP protocol tests:\n");
    test_reqrep_inproc ();
    test_reqrep_tcp ();
    test_rep_send_before_recv ();
    test_reqrecv_before_send ();
    test_reprecv_before_reply ();
    test_req_lb_rotate ();
    test_req_poll_no_pollout_while_waiting ();
    test_rep_poll_pollout_after_recv ();
    test_xreq_xrep_inproc ();
    printf ("All REQ/REP tests passed.\n");
    return 0;
}
