#include "trie.h"
#include "alloc.h"

#include <stdlib.h>
#include <string.h>

struct mb_trie_node {
    uint32_t refcount;
    int subscribed;
    struct mb_trie_node *children[256];
};

static struct mb_trie_node *mb_trie_node_alloc (void)
{
    struct mb_trie_node *n = (struct mb_trie_node *)
        mb_alloc (sizeof (struct mb_trie_node));
    memset (n, 0, sizeof (*n));
    return n;
}

void mb_trie_init (struct mb_trie *self)
{
    self->root = mb_trie_node_alloc ();
}

static void mb_trie_node_term (struct mb_trie_node *n)
{
    int i;

    if (!n)
        return;
    for (i = 0; i < 256; ++i) {
        if (n->children[i])
            mb_trie_node_term (n->children[i]);
    }
    mb_free (n);
}

void mb_trie_term (struct mb_trie *self)
{
    mb_trie_node_term (self->root);
    self->root = NULL;
}

int mb_trie_add (struct mb_trie *self, const void *data, size_t len)
{
    struct mb_trie_node *n = self->root;
    const uint8_t *bytes = (const uint8_t *) data;
    size_t i;
    for (i = 0; i < len; i++) {
        if (!n->children[bytes[i]])
            n->children[bytes[i]] = mb_trie_node_alloc ();
        n->refcount++;
        n = n->children[bytes[i]];
    }
    n->refcount++;
    if (n->subscribed)
        return 0; /* already present */
    n->subscribed = 1;
    return 1; /* newly subscribed */
}

int mb_trie_rm (struct mb_trie *self, const void *data, size_t len)
{
    struct mb_trie_node *n = self->root;
    const uint8_t *bytes = (const uint8_t *) data;
    size_t i;
    for (i = 0; i < len; i++) {
        if (!n->children[bytes[i]])
            return -1;
        n = n->children[bytes[i]];
    }
    if (!n->subscribed)
        return -1;
    n->subscribed = 0;
    return 1; /* removed */
}

int mb_trie_match (struct mb_trie *self, const void *data, size_t len)
{
    struct mb_trie_node *n = self->root;
    const uint8_t *bytes = (const uint8_t *) data;
    size_t i;
    for (i = 0; i < len; i++) {
        if (n->subscribed)
            return 1;
        if (!n->children[bytes[i]])
            break;
        n = n->children[bytes[i]];
    }
    return n->subscribed;
}
