#include "../../src/utils/trie.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

int main (void)
{
    struct mb_trie t;

    assert (mb_trie_init (&t) == 0);

    assert (!mb_trie_match (&t, "hello", 5));

    assert (mb_trie_add (&t, "foo", 3) == 1);
    assert (mb_trie_add (&t, "bar", 3) == 1);

    assert (mb_trie_match (&t, "foo", 3));
    assert (mb_trie_match (&t, "foobar", 6));
    assert (!mb_trie_match (&t, "baz", 3));
    assert (mb_trie_match (&t, "bar", 3));
    assert (mb_trie_match (&t, "bartender", 9));

    mb_trie_add (&t, "", 0);
    assert (mb_trie_match (&t, "anything", 8));
    assert (mb_trie_match (&t, "", 0));

    mb_trie_rm (&t, "", 0);
    assert (!mb_trie_match (&t, "anything", 8));
    assert (mb_trie_match (&t, "foo", 3));

    mb_trie_rm (&t, "foo", 3);
    assert (!mb_trie_match (&t, "foobar", 6));
    assert (mb_trie_match (&t, "bar", 3));

    /* Overlapping prefixes: rm shorter must not break longer. */
    mb_trie_add (&t, "foo", 3);
    mb_trie_add (&t, "foobar", 6);
    mb_trie_rm (&t, "foo", 3);
    assert (!mb_trie_match (&t, "foo", 3));
    assert (!mb_trie_match (&t, "food", 4));
    assert (mb_trie_match (&t, "foobar", 6));
    mb_trie_rm (&t, "foobar", 6);
    assert (!mb_trie_match (&t, "foobar", 6));
    assert (mb_trie_match (&t, "bar", 3));

    /* Duplicate subscribe must be a no-op (refcount must still prune). */
    assert (mb_trie_add (&t, "dup", 3) == 1);
    assert (mb_trie_add (&t, "dup", 3) == 0);
    assert (mb_trie_rm (&t, "dup", 3) == 1);
    assert (!mb_trie_match (&t, "dup", 3));
    assert (mb_trie_rm (&t, "dup", 3) == -ENOENT);

    /* Repeated add/rm should not leave dead nodes that break re-add. */
    {
        int i;
        for (i = 0; i < 100; ++i) {
            assert (mb_trie_add (&t, "sport:news/local", 16) == 1);
            assert (mb_trie_rm (&t, "sport:news/local", 16) == 1);
        }
        assert (!mb_trie_match (&t, "sport:news/local", 16));
        assert (mb_trie_add (&t, "sport:news/local", 16) == 1);
        assert (mb_trie_match (&t, "sport:news/local", 16));
    }

    /* Deep prefixes: term must free the whole subtree (not only root). */
    mb_trie_add (&t, "weather:forecast", 16);
    mb_trie_term (&t);

    /* Re-init after term must work. */
    assert (mb_trie_init (&t) == 0);
    assert (mb_trie_add (&t, "ok", 2) == 1);
    assert (mb_trie_match (&t, "ok", 2));
    mb_trie_term (&t);

    /* Null root: no crash; add reports ENOMEM. */
    t.root = NULL;
    assert (mb_trie_match (&t, "x", 1) == 0);
    assert (mb_trie_rm (&t, "x", 1) == -ENOENT);
    assert (mb_trie_add (&t, "x", 1) == -ENOMEM);

    printf ("test_trie: PASSED\n");
    return 0;
}
