#include "slab.h"
#include "../utils/alloc.h"

#include <stdlib.h>
#include <string.h>

void mb_slab_init (struct mb_slab *self, size_t obj_size, size_t capacity)
{
    size_t i;
    self->obj_size = obj_size;
    self->count = 0;
    self->capacity = capacity;
    self->freelist = (void **) mb_alloc (capacity * sizeof (void *));
    for (i = 0; i < capacity; i++) {
        void *obj = mb_alloc (obj_size);
        memset (obj, 0, obj_size);
        self->freelist[i] = obj;
    }
}

void mb_slab_term (struct mb_slab *self)
{
    size_t i;
    for (i = 0; i < self->capacity; i++) {
        if (self->freelist[i])
            mb_free (self->freelist[i]);
    }
    mb_free (self->freelist);
    self->freelist = NULL;
    self->capacity = 0;
}

void *mb_slab_alloc (struct mb_slab *self)
{
    if (self->count >= self->capacity)
        return NULL;
    void *obj = self->freelist[self->count];
    if (!obj)
        return NULL;
    self->freelist[self->count] = NULL;
    self->count++;
    return obj;
}

void mb_slab_free (struct mb_slab *self, void *obj)
{
    if (!obj)
        return;
    self->count--;
    self->freelist[self->count] = obj;
}
