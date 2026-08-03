#include "../../src/utils/fq.h"
#include "../../src/core/pipe.h"
#include "../../src/transport.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

struct test_pipe {
    struct mb_pipe pipe;
    int recv_calls;
    int recv_rc;
};

static int test_send (struct mb_pipebase *self, struct mb_msg *msg)
{
    (void) self;
    (void) msg;
    return -EAGAIN;
}

static int test_recv (struct mb_pipebase *self, struct mb_msg *msg)
{
    struct test_pipe *pipe = (struct test_pipe *) self;

    (void) msg;
    ++pipe->recv_calls;
    return pipe->recv_rc;
}

static const struct mb_pipebase_vfptr test_vfptr = {
    test_send,
    test_recv,
    NULL,
    NULL,
    NULL
};

static void test_pipe_init (struct test_pipe *pipe, int recv_rc)
{
    pipe->pipe.base.vfptr = &test_vfptr;
    pipe->recv_calls = 0;
    pipe->recv_rc = recv_rc;
}

int main (void)
{
    struct mb_fq fq;
    struct mb_fq_data first_data;
    struct mb_fq_data active_data;
    struct test_pipe first;
    struct test_pipe active;
    struct mb_pipe *received_from = NULL;
    struct mb_msg msg;
    int rc;

    test_pipe_init (&first, 0);
    test_pipe_init (&active, 0);
    mb_fq_init (&fq);
    mb_fq_add (&fq, &first_data, &first.pipe);
    mb_fq_add (&fq, &active_data, &active.pipe);

    mb_fq_activate (&fq, &active_data);
    rc = mb_fq_recv_pipe (&fq, &msg, &received_from);

    assert (rc == 0);
    assert (received_from == active_data.pipe);
    assert (received_from == &active.pipe);
    assert (active.recv_calls == 1);
    assert (first.recv_calls == 0);

    mb_fq_rm (&fq, &active_data);
    mb_fq_rm (&fq, &first_data);
    mb_fq_term (&fq);

    printf ("test_fq_sticky_active: PASSED\n");
    return 0;
}
