#include "../../src/transport/inproc/msgqueue.h"
#include "../../src/memory/msg.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/* Multi-chunk push/pop must not lose messages across GRANULARITY. */
static void test_msgqueue_multi_chunk (void)
{
    struct mb_msgqueue mq;
    struct mb_msg msg;
    int i;
    int n = MB_MSGQUEUE_GRANULARITY + 2;
    char body[8];

    mb_msgqueue_init (&mq, 0);

    for (i = 0; i < n; ++i) {
        body[0] = (char) i;
        mb_msg_init_data (&msg, body, 1);
        assert (mb_msgqueue_push (&mq, &msg) >= 0);
        mb_msg_term (&msg);
    }

    for (i = 0; i < n; ++i) {
        mb_msg_init (&msg, 0);
        assert (!mb_msgqueue_empty (&mq));
        mb_msgqueue_pop (&mq, &msg);
        assert (mb_chunkref_size (&msg.body) == 1);
        assert (((char *) mb_chunkref_data (&msg.body))[0] == (char) i);
        mb_msg_term (&msg);
    }
    assert (mb_msgqueue_empty (&mq));

    mb_msgqueue_term (&mq);
    printf ("  test_msgqueue_multi_chunk: PASSED\n");
}

/*
 * After a full chunk, out.pos may sit at GRANULARITY if the next chunk
 * could not be allocated eagerly. The following push must allocate before
 * writing (not index msgs[GRANULARITY]).
 */
static void test_msgqueue_full_chunk_deferred_alloc (void)
{
    struct mb_msgqueue mq;
    struct mb_msg msg;
    int i;
    char body[8];

    mb_msgqueue_init (&mq, 0);

    for (i = 0; i < MB_MSGQUEUE_GRANULARITY; ++i) {
        body[0] = (char) i;
        mb_msg_init_data (&msg, body, 1);
        assert (mb_msgqueue_push (&mq, &msg) >= 0);
        mb_msg_term (&msg);
    }

    /* Force the deferred-alloc path used when eager alloc failed. */
    if (mq.out.pos == 0 && mq.out.chunk) {
        /* Eager alloc succeeded: walk back to the full chunk sentinel. */
        struct mb_msgqueue_chunk *full = mq.in.chunk;
        assert (full);
        assert (full->next == mq.out.chunk);
        mq.out.chunk = full;
        mq.out.pos = MB_MSGQUEUE_GRANULARITY;
        /* Keep the spare chunk as cache so the next push can reuse it. */
        if (full->next) {
            mq.cache = full->next;
            full->next = NULL;
        }
    }
    assert (mq.out.pos == MB_MSGQUEUE_GRANULARITY);

    body[0] = 'X';
    mb_msg_init_data (&msg, body, 1);
    assert (mb_msgqueue_push (&mq, &msg) >= 0);
    mb_msg_term (&msg);

    assert (mq.out.pos >= 1);
    assert (mq.out.pos <= MB_MSGQUEUE_GRANULARITY);

    for (i = 0; i < MB_MSGQUEUE_GRANULARITY; ++i) {
        mb_msg_init (&msg, 0);
        mb_msgqueue_pop (&mq, &msg);
        assert (((char *) mb_chunkref_data (&msg.body))[0] == (char) i);
        mb_msg_term (&msg);
    }
    mb_msg_init (&msg, 0);
    mb_msgqueue_pop (&mq, &msg);
    assert (((char *) mb_chunkref_data (&msg.body))[0] == 'X');
    mb_msg_term (&msg);
    assert (mb_msgqueue_empty (&mq));

    mb_msgqueue_term (&mq);
    printf ("  test_msgqueue_full_chunk_deferred_alloc: PASSED\n");
}

/* maxmem must account for the message being pushed, not only current mem. */
static void test_msgqueue_maxmem_includes_push (void)
{
    struct mb_msgqueue mq;
    struct mb_msg msg;
    char body[64];

    memset (body, 'x', sizeof (body));
    mb_msgqueue_init (&mq, 32);

    mb_msg_init_data (&msg, body, 20);
    assert (mb_msgqueue_push (&mq, &msg) >= 0);
    mb_msg_term (&msg);
    assert (mq.mem == 20);

    mb_msg_init_data (&msg, body, 20);
    assert (mb_msgqueue_push (&mq, &msg) == -EAGAIN);
    mb_msg_term (&msg);
    assert (mq.mem == 20);

    mb_msg_init (&msg, 0);
    mb_msgqueue_pop (&mq, &msg);
    mb_msg_term (&msg);
    assert (mq.mem == 0);

    mb_msg_init_data (&msg, body, 64);
    assert (mb_msgqueue_push (&mq, &msg) == -EAGAIN);
    mb_msg_term (&msg);
    assert (mq.mem == 0);
    assert (mb_msgqueue_empty (&mq));

    mb_msgqueue_term (&mq);
    printf ("  test_msgqueue_maxmem_includes_push: PASSED\n");
}

/* can_push must not stay true when the next fixed-size push cannot fit. */
static void test_msgqueue_can_push_matches_size (void)
{
    struct mb_msgqueue mq;
    struct mb_msg msg;
    char body[32];

    memset (body, 'y', sizeof (body));
    mb_msgqueue_init (&mq, 65);

    mb_msg_init_data (&msg, body, 32);
    assert (mb_msgqueue_push (&mq, &msg) >= 0);
    mb_msg_term (&msg);
    mb_msg_init_data (&msg, body, 32);
    assert (mb_msgqueue_push (&mq, &msg) >= 0);
    mb_msg_term (&msg);
    assert (mq.mem == 64);

    /* 1 byte free — can_push_sz(32) false; after failed push, can_push too. */
    assert (mb_msgqueue_can_push_sz (&mq, 32) == 0);
    assert (mb_msgqueue_can_push_sz (&mq, 1) == 1);

    mb_msg_init_data (&msg, body, 32);
    assert (mb_msgqueue_push (&mq, &msg) == -EAGAIN);
    mb_msg_term (&msg);
    assert (mb_msgqueue_can_push (&mq) == 0);
    assert (mb_msgqueue_can_push_sz (&mq, 32) == 0);

    mb_msg_init (&msg, 0);
    mb_msgqueue_pop (&mq, &msg);
    mb_msg_term (&msg);
    assert (mq.mem == 32);
    assert (mb_msgqueue_can_push (&mq) == 1);
    assert (mb_msgqueue_can_push_sz (&mq, 32) == 1);

    mb_msgqueue_term (&mq);
    printf ("  test_msgqueue_can_push_matches_size: PASSED\n");
}

int main (void)
{
    printf ("msgqueue tests:\n");
    test_msgqueue_multi_chunk ();
    test_msgqueue_full_chunk_deferred_alloc ();
    test_msgqueue_maxmem_includes_push ();
    test_msgqueue_can_push_matches_size ();
    printf ("All msgqueue tests passed.\n");
    return 0;
}
