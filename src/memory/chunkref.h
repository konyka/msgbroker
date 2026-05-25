#ifndef MB_CHUNKREF_H_INCLUDED
#define MB_CHUNKREF_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#define MB_CHUNKREF_MAX 64

struct mb_chunkref {
    size_t size;
    union {
        uint8_t data [MB_CHUNKREF_MAX];
        void *chunk;
    } u;
};

void mb_chunkref_init (struct mb_chunkref *self, size_t size);
void mb_chunkref_init_chunk (struct mb_chunkref *self, void *chunk,
    size_t offset, size_t size);
void mb_chunkref_term (struct mb_chunkref *self);
void mb_chunkref_mv (struct mb_chunkref *dst, struct mb_chunkref *src);
void mb_chunkref_cp (struct mb_chunkref *dst, struct mb_chunkref *src);
void *mb_chunkref_data (struct mb_chunkref *self);
size_t mb_chunkref_size (struct mb_chunkref *self);
void mb_chunkref_set (struct mb_chunkref *self, const void *data,
    size_t size);
void mb_chunkref_resize (struct mb_chunkref *self, size_t size);

#endif
