#include "trie.h"
#include "alloc.h"

#include <errno.h>
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
    if (!n)
        return NULL;
    memset (n, 0, sizeof (*n));
    return n;
}

int mb_trie_init (struct mb_trie *self)
{
    self->root = mb_trie_node_alloc ();
    if (!self->root)
        return -ENOMEM;
    return 0;
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

    if (!n)
        return -ENOMEM;

    /* Ensure the path exists, then reject duplicates without bumping refs. */
    for (i = 0; i < len; i++) {
        if (!n->children[bytes[i]]) {
            n->children[bytes[i]] = mb_trie_node_alloc ();
            if (!n->children[bytes[i]])
                return -ENOMEM;
        }
        n = n->children[bytes[i]];
    }
    if (n->subscribed)
        return 0;

    n = self->root;
    for (i = 0; i < len; i++) {
        n->refcount++;
        n = n->children[bytes[i]];
    }
    n->refcount++;
    n->subscribed = 1;
    return 1;
}

static int mb_trie_rm_rec (struct mb_trie_node *n, const uint8_t *data,
    size_t len)
{
    struct mb_trie_node *child;
    int rc;

    if (len == 0) {
        if (!n->subscribed)
            return -ENOENT;
        n->subscribed = 0;
        if (n->refcount > 0)
            n->refcount--;
        return 1;
    }

    child = n->children[data[0]];
    if (!child)
        return -ENOENT;

    rc = mb_trie_rm_rec (child, data + 1, len - 1);
    if (rc < 0)
        return rc;

    if (n->refcount > 0)
        n->refcount--;

    if (child->refcount == 0 && !child->subscribed) {
        n->children[data[0]] = NULL;
        mb_trie_node_term (child);
    }
    return 1;
}

int mb_trie_rm (struct mb_trie *self, const void *data, size_t len)
{
    if (!self->root)
        return -ENOENT;
    return mb_trie_rm_rec (self->root, (const uint8_t *) data, len);
}

int mb_trie_match (struct mb_trie *self, const void *data, size_t len)
{
    struct mb_trie_node *n = self->root;
    const uint8_t *bytes = (const uint8_t *) data;
    size_t i;

    if (!n)
        return 0;

    for (i = 0; i < len; i++) {
        if (n->subscribed)
            return 1;
        if (!n->children[bytes[i]])
            break;
        n = n->children[bytes[i]];
    }
    return n->subscribed;
}
