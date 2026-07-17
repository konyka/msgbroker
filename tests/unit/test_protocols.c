#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>
#include <msgbroker/mb_bus.h>
#include <msgbroker/mb_survey.h>

/*  PUB->SUB via inproc: broadcast. */
static void test_pubsub_inproc (void)
{
    int pub, sub;
    int rc;
    char buf[64];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_bind (pub, "inproc://pubsub1");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://pubsub1");
    assert (rc >= 0);

    /*  PUB sends, SUB receives. */
    rc = mb_send (pub, "NEWS", 4, 0);
    assert (rc == 4);

    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "NEWS", 4) == 0);

    rc = mb_close (pub);
    assert (rc == 0);
    rc = mb_close (sub);
    assert (rc == 0);

    printf ("  test_pubsub_inproc: PASSED\n");
}

/*  PUB cannot recv (NORECV flag). */
static void test_pub_norecv (void)
{
    int pub;
    char buf[64];
    int rc;

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);

    rc = mb_recv (pub, buf, sizeof (buf), 0);
    assert (rc < 0);
    assert (mb_errno () == ENOTSUP);

    rc = mb_close (pub);
    assert (rc == 0);

    printf ("  test_pub_norecv: PASSED\n");
}

/*  SUB cannot send (NOSEND flag). */
static void test_sub_nosend (void)
{
    int sub;
    int rc;

    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_send (sub, "X", 1, 0);
    assert (rc < 0);
    assert (mb_errno () == ENOTSUP);

    rc = mb_close (sub);
    assert (rc == 0);

    printf ("  test_sub_nosend: PASSED\n");
}

/*  BUS: all sockets receive broadcast. */
static void test_bus_inproc (void)
{
    int b1, b2, b3;
    int rc;
    char buf[64];

    b1 = mb_socket (AF_MB, MB_BUS);
    assert (b1 >= 0);
    b2 = mb_socket (AF_MB, MB_BUS);
    assert (b2 >= 0);
    b3 = mb_socket (AF_MB, MB_BUS);
    assert (b3 >= 0);

    rc = mb_bind (b1, "inproc://bus1");
    assert (rc >= 0);
    rc = mb_connect (b2, "inproc://bus1");
    assert (rc >= 0);
    rc = mb_connect (b3, "inproc://bus1");
    assert (rc >= 0);

    /*  b1 sends, b2 and b3 should receive. */
    rc = mb_send (b1, "BUSY", 4, 0);
    assert (rc == 4);

    int got = 0;
    rc = mb_recv (b2, buf, sizeof (buf), 0);
    if (rc == 4) got++;
    rc = mb_recv (b3, buf, sizeof (buf), 0);
    if (rc == 4) got++;

    assert (got == 2);

    rc = mb_close (b1);
    assert (rc == 0);
    rc = mb_close (b2);
    assert (rc == 0);
    rc = mb_close (b3);
    assert (rc == 0);

    printf ("  test_bus_inproc: PASSED\n");
}

/*  SURVEYOR->RESPONDENT: survey/response. */
static void test_survey_inproc (void)
{
    int sv, resp;
    int rc;
    char buf[64];

    sv = mb_socket (AF_MB, MB_SURVEYOR);
    assert (sv >= 0);
    resp = mb_socket (AF_MB, MB_RESPONDENT);
    assert (resp >= 0);

    rc = mb_bind (sv, "inproc://survey1");
    assert (rc >= 0);
    rc = mb_connect (resp, "inproc://survey1");
    assert (rc >= 0);

    /*  Surveyor sends survey. */
    rc = mb_send (sv, "Q1", 2, 0);
    assert (rc == 2);

    /*  Respondent receives survey. */
    rc = mb_recv (resp, buf, sizeof (buf), 0);
    assert (rc == 2);
    assert (memcmp (buf, "Q1", 2) == 0);

    /*  Respondent sends response. */
    rc = mb_send (resp, "A1", 2, 0);
    assert (rc == 2);

    /*  Surveyor receives response. */
    rc = mb_recv (sv, buf, sizeof (buf), 0);
    assert (rc == 2);
    assert (memcmp (buf, "A1", 2) == 0);

    rc = mb_close (sv);
    assert (rc == 0);
    rc = mb_close (resp);
    assert (rc == 0);

    printf ("  test_survey_inproc: PASSED\n");
}

/*  Surveyor cannot recv before sending a survey (EFSM). */
static void test_survey_fsm (void)
{
    int sv;
    char buf[64];
    int rc;

    sv = mb_socket (AF_MB, MB_SURVEYOR);
    assert (sv >= 0);

    rc = mb_recv (sv, buf, sizeof (buf), 0);
    assert (rc < 0);
    assert (mb_errno () == EFSM);

    rc = mb_close (sv);
    assert (rc == 0);

    printf ("  test_survey_fsm: PASSED\n");
}

/*  Survey with no respondents must not enter surveying FSM. */
static void test_survey_send_no_peers (void)
{
    int sv;
    int rc;

    sv = mb_socket (AF_MB, MB_SURVEYOR);
    assert (sv >= 0);

    rc = mb_send (sv, "Q", 1, MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EAGAIN);

    /*  Still able to send once a peer appears (not stuck in surveying). */
    rc = mb_send (sv, "Q", 1, MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EAGAIN);

    rc = mb_close (sv);
    assert (rc == 0);

    printf ("  test_survey_send_no_peers: PASSED\n");
}

/*  Raw XSURVEYOR must not report success when nothing was delivered. */
static void test_xsurveyor_send_no_peers (void)
{
    int sv;
    int rc;

    sv = mb_socket (AF_MB, MB_XSURVEYOR);
    assert (sv >= 0);

    rc = mb_send (sv, "Q", 1, MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EAGAIN);

    rc = mb_close (sv);
    assert (rc == 0);

    printf ("  test_xsurveyor_send_no_peers: PASSED\n");
}

int main (void)
{
    printf ("PUB/SUB, BUS, Survey protocol tests:\n");
    test_pubsub_inproc ();
    test_pub_norecv ();
    test_sub_nosend ();
    test_bus_inproc ();
    test_survey_inproc ();
    test_survey_fsm ();
    test_survey_send_no_peers ();
    test_xsurveyor_send_no_peers ();
    printf ("All protocol tests passed.\n");
    return 0;
}
