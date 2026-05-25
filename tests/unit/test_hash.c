#include "../../src/utils/hash.h"
#include "../../src/utils/cont.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct test_item {
    uint32_t key;
    int val;
    struct mb_hash_item item;
};

int main (void)
{
    struct mb_hash h;
    struct test_item a, b, c;

    mb_hash_init (&h, 16);
    assert (mb_hash_count (&h) == 0);

    a.key = 100; a.val = 1;
    b.key = 200; b.val = 2;
    c.key = 116; c.val = 3;

    mb_hash_insert (&h, 100, &a.item);
    mb_hash_insert (&h, 200, &b.item);
    mb_hash_insert (&h, 116, &c.item);
    assert (mb_hash_count (&h) == 3);

    struct mb_hash_item *hi = mb_hash_find (&h, 100);
    assert (hi != NULL);
    struct test_item *ti = (struct test_item *) ((char *) hi - ((size_t) &((struct test_item *) 0)->item));
    assert (ti->val == 1);
    (void) ti;

    hi = mb_hash_find (&h, 200);
    assert (hi != NULL);

    hi = mb_hash_find (&h, 999);
    assert (hi == NULL);

    mb_hash_erase (&h, 100);
    assert (mb_hash_count (&h) == 2);
    hi = mb_hash_find (&h, 100);
    assert (hi == NULL);

    hi = mb_hash_find (&h, 116);
    ti = (struct test_item *) ((char *) hi - ((size_t) &((struct test_item *) 0)->item));
    assert (ti->val == 3);

    printf ("test_hash: PASSED\n");
    mb_hash_term (&h);
    return 0;
}
