#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include "../../src/aio/coroutine.h"

static void coro_simple_fn (void *arg)
{
    int *flag = (int *) arg;
    *flag = 42;
}

static void test_coro_basic (void)
{
    struct mb_coro *coro;
    int flag = 0;

    coro = mb_coro_create (coro_simple_fn, &flag);
    assert (coro != NULL);

    int rc = mb_coro_resume (coro);
    assert (rc == 1);
    assert (flag == 42);
    assert (mb_coro_done (coro));

    mb_coro_destroy (coro);

    printf ("  coro_basic: OK\n");
}

struct coro_io_ctx {
    int sock;
    const char *send_data;
    char recv_buf[64];
    int send_rc;
    int recv_rc;
    int done;
};

static void coro_io_fn (void *arg)
{
    struct coro_io_ctx *ctx = (struct coro_io_ctx *) arg;

    ctx->send_rc = mb_coro_send (ctx->sock, ctx->send_data,
        strlen (ctx->send_data));

    memset (ctx->recv_buf, 0, sizeof (ctx->recv_buf));
    ctx->recv_rc = mb_coro_recv (ctx->sock, ctx->recv_buf,
        sizeof (ctx->recv_buf));

    ctx->done = 1;
}

static void test_coro_sendrecv (void)
{
    int s1, s2, rc;
    struct mb_coro *coro;
    struct coro_io_ctx ctx;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://coro-io");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://coro-io");
    assert (rc >= 0);

    memset (&ctx, 0, sizeof (ctx));
    ctx.sock = s2;
    ctx.send_data = "HELLO";

    coro = mb_coro_create (coro_io_fn, &ctx);
    assert (coro != NULL);

    rc = mb_coro_resume (coro);
    assert (ctx.send_rc == 5);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_coro_resume (coro);
    assert (rc == 1);
    assert (ctx.recv_rc == 5);
    assert (memcmp (ctx.recv_buf, "WORLD", 5) == 0);
    assert (ctx.done == 1);
    assert (mb_coro_done (coro));

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
