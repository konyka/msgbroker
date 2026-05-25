#include "../../src/memory/msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

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

    printf ("test_msg: PASSED\n");
    return 0;
}
