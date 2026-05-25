#include "../../src/utils/list.h"
#include "../../src/utils/cont.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct test_item {
    int val;
    struct mb_list_item item;
};

int main (void)
{
    struct mb_list list;
    struct test_item items [5];
    int i;

    mb_list_init (&list);
    assert (mb_list_empty (&list));
    assert (mb_list_begin (&list) == NULL);

    for (i = 0; i < 5; i++) {
        items[i].val = i + 1;
        mb_list_item_init (&items[i].item);
        mb_list_insert (&list, &items[i].item, NULL);
    }
    assert (!mb_list_empty (&list));

    i = 1;
    struct mb_list_item *it = mb_list_begin (&list);
    while (it != mb_list_end (&list)) {
        struct test_item *ti = mb_cont (it, struct test_item, item);
        assert (ti->val == i);
        (void) ti;
        i++;
        it = mb_list_next (&list, it);
    }
    assert (i == 6);

    mb_list_erase (&list, &items[2].item);
    assert (!mb_list_item_isinlist (&items[2].item));

    it = mb_list_begin (&list);
    struct test_item *ti = mb_cont (it, struct test_item, item);
    assert (ti->val == 1);
    (void) ti;
    it = mb_list_next (&list, it);
    ti = mb_cont (it, struct test_item, item);
    assert (ti->val == 2);
    (void) ti;
    it = mb_list_next (&list, it);
    ti = mb_cont (it, struct test_item, item);
    assert (ti->val == 4);
    (void) ti;

    printf ("test_list: PASSED\n");
    mb_list_term (&list);
    return 0;
}
