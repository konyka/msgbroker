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

/*  T-LIMITS: process-wide security caps.

    Validates:
      - The default rcvbuf cap is 16 MiB; setting MB_RCVBUF to any value
        strictly greater than the cap returns -EPERM (via mb_setsockopt
        returning -1 and mb_errno() == EPERM).
      - Setting MB_RCVBUF to a value <= cap succeeds.
      - Tightening the cap below an already-set value works (we never
        retroactively shrink a stored value; the user must lower it
        explicitly via a follow-up setsockopt that itself passes the
        new cap — this test exercises only the path of the cap being
        tightened below the *current* rcvbuf, which the setopt check
        also enforces).
      - SND / RCV timeout caps default to 60 s; setting > cap -> -EPERM.
      - Setting MB_LIMITS_* itself is idempotent and accepts the
        defaults (must not return -ENOPROTOOPT). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

/* Default caps (mirrored from src/core/limits.c). Kept in this header
 * block as a sanity check that the public surface matches the
 * implementation. If the implementation moves the default values, this
 * test must move with them or it loses its meaning. */
#define MB_LIMITS_TEST_RCVBUF_DEFAULT  (16 * 1024 * 1024)
#define MB_LIMITS_TEST_TIMEO_DEFAULT   (60 * 1000)
#define MB_LIMITS_TEST_BACKLOG_DEFAULT 128

/* Above-cap value. Default cap is 16 MiB so 32 MiB is guaranteed to
 * exceed it. INT_MAX is acceptable for sndtimeo/rcvtimeo too: the cap
 * is in milliseconds (60_000 ms default). */
#define MB_LIMITS_TEST_OVER  (32 * 1024 * 1024)

static void test_limits_rcvbuf_default_cap (void)
{
    int s, rc;
    int over = MB_LIMITS_TEST_OVER;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /* Setting rcvbuf above the default cap must be rejected with -EPERM. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &over, sizeof (over));
    assert (rc == -1);
    assert (mb_errno () == EPERM);

    mb_close (s);

    printf ("  test_limits_rcvbuf_default_cap: PASSED\n");
}

static void test_limits_rcvbuf_at_cap_ok (void)
{
    int s, rc;
    int at_cap = MB_LIMITS_TEST_RCVBUF_DEFAULT;
    int under_cap = MB_LIMITS_TEST_RCVBUF_DEFAULT / 2;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /* Exactly at the cap is allowed. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &at_cap,
        sizeof (at_cap));
    assert (rc == 0);

    /* Well below the cap is allowed. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &under_cap,
        sizeof (under_cap));
    assert (rc == 0);

    mb_close (s);

    printf ("  test_limits_rcvbuf_at_cap_ok: PASSED\n");
}

static void test_limits_rcvbuf_tighten_rejects (void)
{
    int s, rc;
    int tight_cap = 64 * 1024;       /* 64 KiB */
    int over_tight = 128 * 1024;     /* 128 KiB */

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /* Tighten the rcvbuf cap down to 64 KiB. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LIMITS_RCVBUF, &tight_cap,
        sizeof (tight_cap));
    assert (rc == 0);

    /* Now any value above 64 KiB must be rejected with -EPERM. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &over_tight,
        sizeof (over_tight));
    assert (rc == -1);
    assert (mb_errno () == EPERM);

    mb_close (s);

    printf ("  test_limits_rcvbuf_tighten_rejects: PASSED\n");
}

static void test_limits_sndtimeo_above_cap (void)
{
    int s, rc;
    int above = INT32_MAX;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &above,
        sizeof (above));
    assert (rc == -1);
    assert (mb_errno () == EPERM);

    mb_close (s);

    printf ("  test_limits_sndtimeo_above_cap: PASSED\n");
}

static void test_limits_rcvtimeo_above_cap (void)
{
    int s, rc;
    int above = INT32_MAX;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &above,
        sizeof (above));
    assert (rc == -1);
    assert (mb_errno () == EPERM);

    mb_close (s);

    printf ("  test_limits_rcvtimeo_above_cap: PASSED\n");
}

static void test_limits_setters_accept_defaults (void)
{
    int s, rc;
    int v;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /* Each MB_LIMITS_* setter must recognise its option id. Setting
     * the default value is a no-op-equivalent and must succeed. */
    v = MB_LIMITS_TEST_RCVBUF_DEFAULT;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LIMITS_RCVBUF, &v,
        sizeof (v));
    assert (rc == 0);

    v = MB_LIMITS_TEST_TIMEO_DEFAULT;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LIMITS_SNDTIMEO, &v,
        sizeof (v));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LIMITS_RCVTIMEO, &v,
        sizeof (v));
    assert (rc == 0);

    v = MB_LIMITS_TEST_BACKLOG_DEFAULT;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LIMITS_BACKLOG, &v,
        sizeof (v));
    assert (rc == 0);

    mb_close (s);

    printf ("  test_limits_setters_accept_defaults: PASSED\n");
}

int main (void)
{
    printf ("test_limits:\n");
    test_limits_setters_accept_defaults ();
    test_limits_rcvbuf_default_cap ();
    test_limits_rcvbuf_at_cap_ok ();
    test_limits_rcvbuf_tighten_rejects ();
    test_limits_sndtimeo_above_cap ();
    test_limits_rcvtimeo_above_cap ();
    printf ("test_limits: PASSED\n");
    return 0;
}
