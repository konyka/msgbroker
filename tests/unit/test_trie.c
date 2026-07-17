#include "../../src/utils/trie.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    struct mb_trie t;

    mb_trie_init (&t);

    assert (!mb_trie_match (&t, "hello", 5));

    mb_trie_add (&t, "foo", 3);
    mb_trie_add (&t, "bar", 3);

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

    /* Deep prefixes: term must free the whole subtree (not only root). */
    mb_trie_add (&t, "sport:news/local", 16);
    mb_trie_add (&t, "weather:forecast", 16);
    mb_trie_term (&t);

    /* Re-init after term must work. */
    mb_trie_init (&t);
    mb_trie_add (&t, "ok", 2);
    assert (mb_trie_match (&t, "ok", 2));
    mb_trie_term (&t);

    printf ("test_trie: PASSED\n");
    return 0;
}
