#include "../../src/memory/msg.h"
#include "../../src/memory/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

static void test_init_data_oom (void)
{
    void *probe = NULL;
    size_t huge = (size_t) -1 / 4;
    char b = 'x';
    struct mb_msg oom;
    int arc;

    /* Only exercise the OOM path when the platform rejects this size. */
    arc = mb_chunk_alloc (huge, &probe);
    if (arc != -ENOMEM) {
        if (probe)
            mb_chunk_free (probe);
        printf ("  init_data_oom: SKIPPED\n");
        return;
    }

    mb_msg_init_data (&oom, &b, huge);
    assert (mb_chunkref_data (&oom.body) == NULL);
    assert (mb_chunkref_size (&oom.body) == huge);
    mb_msg_term (&oom);
    printf ("  init_data_oom: OK\n");
}

static void test_msg_init_size_oom (void)
{
    void *probe = NULL;
    size_t huge = (size_t) -1 / 4;
    struct mb_msg msg;
    int arc;

    arc = mb_chunk_alloc (huge, &probe);
    if (arc != -ENOMEM) {
        if (probe)
            mb_chunk_free (probe);
        printf ("  msg_init_size_oom: SKIPPED\n");
        return;
    }

    assert (mb_msg_init_size (&msg, huge) == -ENOMEM);
    assert (mb_chunkref_size (&msg.body) == 0);
    mb_msg_term (&msg);
    printf ("  msg_init_size_oom: OK\n");
}

static void test_chunkref_set_oom (void)
{
    void *probe = NULL;
    size_t huge = (size_t) -1 / 4;
    char keep[] = "keep";
    char b = 'x';
    struct mb_chunkref cr;
    int arc;

    arc = mb_chunk_alloc (huge, &probe);
    if (arc != -ENOMEM) {
        if (probe)
            mb_chunk_free (probe);
        printf ("  chunkref_set_oom: SKIPPED\n");
        return;
    }

    mb_chunkref_init (&cr, 0);
    assert (mb_chunkref_set (&cr, keep, sizeof (keep)) == 0);
    assert (mb_chunkref_size (&cr) == sizeof (keep));
    assert (memcmp (mb_chunkref_data (&cr), keep, sizeof (keep)) == 0);

    assert (mb_chunkref_set (&cr, &b, huge) == -ENOMEM);
    assert (mb_chunkref_size (&cr) == sizeof (keep));
    assert (memcmp (mb_chunkref_data (&cr), keep, sizeof (keep)) == 0);

    mb_chunkref_term (&cr);
    printf ("  chunkref_set_oom: OK\n");
}

int main (void)
{
    struct mb_msg m1, m2;

    mb_msg_init (&m1, 64);
    void *body = mb_chunkref_data (&m1.body);
    memset (body, 'X', 64);

    mb_msg_cp (&m2, &m1);
    void *body2 = mb_chunkref_data (&m2.body);
    assert (memcmp (body, body2, 64) == 0);

    struct mb_msg m3;
    mb_msg_mv (&m3, &m1);
    assert (mb_chunkref_size (&m3.body) == 64);
    assert (mb_chunkref_size (&m1.body) == 0);

    mb_msg_term (&m2);
    mb_msg_term (&m3);

    test_init_data_oom ();
    test_msg_init_size_oom ();
    test_chunkref_set_oom ();

    printf ("test_msg: PASSED\n");
    return 0;
}
