#ifndef MB_TRIE_H_INCLUDED
#define MB_TRIE_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

struct mb_trie_node;

struct mb_trie {
    struct mb_trie_node *root;
};

void mb_trie_init (struct mb_trie *self);
void mb_trie_term (struct mb_trie *self);
int mb_trie_add (struct mb_trie *self, const void *data, size_t len);
int mb_trie_rm (struct mb_trie *self, const void *data, size_t len);
int mb_trie_match (struct mb_trie *self, const void *data, size_t len);

#endif
