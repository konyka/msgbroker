#ifndef MB_SLAB_H_INCLUDED
#define MB_SLAB_H_INCLUDED

#include <stddef.h>

struct mb_slab {
    size_t obj_size;
    size_t count;
    size_t capacity;
    void **freelist;
};

void mb_slab_init (struct mb_slab *self, size_t obj_size, size_t capacity);
void mb_slab_term (struct mb_slab *self);
void *mb_slab_alloc (struct mb_slab *self);
void mb_slab_free (struct mb_slab *self, void *obj);

#endif
