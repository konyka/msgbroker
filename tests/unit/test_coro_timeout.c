/*  T-CORO-TIMEOUT: mb_coro_send/mb_coro_recv honour MB_SNDTIMEO/MB_RCVTIMEO.
 *
 *  Mirrors tests/unit/test_send_recv_timeout.c but drives send/recv from
 *  inside a coroutine via mb_coro_send/mb_coro_recv. When the call would
 *  block longer than the configured timeout, the call returns -1 with
 *  errno=ETIMEDOUT instead of yielding forever.
 *
 *  Refs: T-CORO-TIMEOUT
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include "../../src/aio/coroutine.h"

static long elapsed_ms (struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000L +
        (t1.tv_nsec - t0.tv_nsec) / 1000000L;
}

/* Hard wall-clock guard so a regression on the timeout path does not hang
 * the whole ctest run; the assertion fires at 5s with a clear message. */
static void drive_until_done (struct mb_coro *coro, long budget_ms,
    struct timespec t0)
{
    struct timespec now;
    while (!mb_coro_done (coro)) {
        clock_gettime (CLOCK_MONOTONIC, &now);
        if (elapsed_ms (t0, now) > budget_ms) {
            fprintf (stderr,
                "  coro driver exceeded %ldms budget, coro stuck (timeout not honoured)\n",
                budget_ms);
            assert (0);
        }
        mb_coro_resume (coro);
    }
}

struct coro_timeout_ctx {
    int sock;
    char buf[16];
    int recv_rc;
    int recv_errno;
    int done;
};

static void coro_recv_fn (void *arg)
{
    struct coro_timeout_ctx *ctx = (struct coro_timeout_ctx *) arg;

    ctx->recv_rc = mb_coro_recv (ctx->sock, ctx->buf, sizeof (ctx->buf));
    ctx->recv_errno = mb_errno ();

    ctx->done = 1;
}

/* Coroutine recv with RCVTIMEO=50ms and no peer: must return -1/ETIMEDOUT
 * within the budget instead of yielding forever. */
static void test_coro_recv_timeout (void)
{
    int s;
    int val;
    int rc;
    struct mb_coro *coro;
    struct coro_timeout_ctx ctx;
    struct timespec t0, t1;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 50;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, sizeof (val));
    assert (rc == 0);

    memset (&ctx, 0, sizeof (ctx));
    ctx.sock = s;

    coro = mb_coro_create (coro_recv_fn, &ctx);
    assert (coro != NULL);

    clock_gettime (CLOCK_MONOTONIC, &t0);
    drive_until_done (coro, 5000, t0);
    clock_gettime (CLOCK_MONOTONIC, &t1);

    assert (ctx.recv_rc == -1);
    assert (ctx.recv_errno == ETIMEDOUT);
    /* Polling sleeps 1ms per tick, so 50ms timeout returns well under 500ms. */
    assert (elapsed_ms (t0, t1) < 500);
    assert (ctx.done == 1);
    assert (mb_coro_done (coro));

    mb_coro_destroy (coro);
    rc = mb_close (s);
    assert (rc == 0);

    printf ("  coro_recv_timeout: PASSED\n");
}

struct coro_send_ctx {
    int sock;
    int send_rc;
    int send_errno;
    int done;
};

static void coro_send_fn (void *arg)
{
    struct coro_send_ctx *ctx = (struct coro_send_ctx *) arg;

    ctx->send_rc = mb_coro_send (ctx->sock, "X", 1);
    ctx->send_errno = mb_errno ();

    ctx->done = 1;
}

/* Coroutine send with SNDTIMEO=50ms and no peer: must return -1/ETIMEDOUT. */
static void test_coro_send_timeout (void)
{
    int s;
    int val;
    int rc;
    struct mb_coro *coro;
    struct coro_send_ctx ctx;
    struct timespec t0, t1;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 50;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, sizeof (val));
    assert (rc == 0);

    memset (&ctx, 0, sizeof (ctx));
    ctx.sock = s;

    coro = mb_coro_create (coro_send_fn, &ctx);
    assert (coro != NULL);

    clock_gettime (CLOCK_MONOTONIC, &t0);
    drive_until_done (coro, 5000, t0);
    clock_gettime (CLOCK_MONOTONIC, &t1);

    assert (ctx.send_rc == -1);
    assert (ctx.send_errno == ETIMEDOUT);
    assert (elapsed_ms (t0, t1) < 500);
    assert (ctx.done == 1);
    assert (mb_coro_done (coro));

    mb_coro_destroy (coro);
    rc = mb_close (s);
    assert (rc == 0);

    printf ("  coro_send_timeout: PASSED\n");
}

int main (void)
{
    printf ("test_coro_timeout:\n");
    test_coro_recv_timeout ();
    test_coro_send_timeout ();
    printf ("test_coro_timeout: PASSED\n");
    return 0;
}
