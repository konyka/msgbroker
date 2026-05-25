#include "../../src/utils/queue.h"
#include "../../src/utils/cont.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct test_item {
    int val;
    struct mb_queue_item item;
};

int main (void)
{
    struct mb_queue q;
    struct test_item items [3];
    int i;

    mb_queue_init (&q);
    assert (mb_queue_empty (&q));

    for (i = 0; i < 3; i++) {
        items[i].val = i + 10;
        mb_queue_item_init (&items[i].item);
        mb_queue_push (&q, &items[i].item);
    }
    assert (!mb_queue_empty (&q));

    struct mb_queue_item *qi = mb_queue_pop (&q);
    struct test_item *ti = mb_cont (qi, struct test_item, item);
    assert (ti->val == 10);
    (void) ti;

    qi = mb_queue_pop (&q);
    ti = mb_cont (qi, struct test_item, item);
    assert (ti->val == 11);
    (void) ti;

    qi = mb_queue_pop (&q);
    ti = mb_cont (qi, struct test_item, item);
    assert (ti->val == 12);
    (void) ti;

    qi = mb_queue_pop (&q);
    assert (qi == NULL);
    assert (mb_queue_empty (&q));

    printf ("test_queue: PASSED\n");
    mb_queue_term (&q);
    return 0;
}
