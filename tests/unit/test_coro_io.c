#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include "../../src/aio/coroutine.h"

struct coro_ctx {
    int sock;
    const char *send_data;
    char recv_buf[64];
    int recv_len;
    int phase;
};

static void coro_fn (void *arg)
{
    struct coro_ctx *ctx = (struct coro_ctx *) arg;
    int rc;

    ctx->phase = 1;
    rc = mb_coro_send (ctx->sock, ctx->send_data,
        strlen (ctx->send_data));
    assert (rc == (int) strlen (ctx->send_data));

    ctx->phase = 2;
    memset (ctx->recv_buf, 0, sizeof (ctx->recv_buf));
    rc = mb_coro_recv (ctx->sock, ctx->recv_buf,
        sizeof (ctx->recv_buf));
    assert (rc > 0);
    ctx->recv_len = rc;

    ctx->phase = 3;
}

static void test_coro_basic (void)
{
    struct mb_coro *coro;
    struct coro_ctx ctx;
    int rc;

    ctx.sock = -1;
    ctx.send_data = "PING";
    ctx.recv_len = 0;
    ctx.phase = 0;

    coro = mb_coro_create (coro_fn, &ctx);
    assert (coro != NULL);

    rc = mb_coro_resume (coro);
    assert (rc == 1);
    assert (mb_coro_done (coro));

    mb_coro_destroy (coro);

    printf ("  coro_basic: OK\n");
}

static void test_coro_sendrecv (void)
{
    int s1, s2, rc;
    struct mb_coro *coro;
    struct coro_ctx ctx;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://coro");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://coro");
    assert (rc >= 0);

    ctx.sock = s2;
    ctx.send_data = "HELLO";
    ctx.recv_len = 0;
    ctx.phase = 0;

    coro = mb_coro_create (coro_fn, &ctx);
    assert (coro != NULL);

    rc = mb_coro_resume (coro);
    assert (rc == 1);
    assert (ctx.phase == 3);
    assert (mb_coro_done (coro));

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    mb_coro_destroy (coro);
    mb_close (s2);
    mb_close (s1);

    printf ("  coro_sendrecv: OK\n");
}

static void test_coro_create_destroy (void)
{
    struct mb_coro *coro;
    int i;

    for (i = 0; i < 100; i++) {
        coro = mb_coro_create (NULL, NULL);
        assert (coro != NULL);
        mb_coro_destroy (coro);
    }

    printf ("  coro_create_destroy: OK\n");
}

int main (void)
{
    printf ("test_coro_io:\n");
    test_coro_create_destroy ();
    test_coro_basic ();
    test_coro_sendrecv ();
    printf ("test_coro_io: PASSED\n");
    return 0;
}
