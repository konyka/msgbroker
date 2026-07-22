#ifndef MB_MSG_H_INCLUDED
#define MB_MSG_H_INCLUDED

#include "chunkref.h"

struct mb_msg {
    struct mb_chunkref sphdr;
    struct mb_chunkref hdrs;
    struct mb_chunkref body;
};

void mb_msg_init (struct mb_msg *self, size_t size);
/* Like mb_msg_init, but returns -ENOMEM if a large body cannot be allocated. */
int mb_msg_init_size (struct mb_msg *self, size_t size);
void mb_msg_init_data (struct mb_msg *self, const void *data, size_t size);
void mb_msg_init_chunk (struct mb_msg *self, void *chunk);
void mb_msg_term (struct mb_msg *self);
void mb_msg_mv (struct mb_msg *dst, struct mb_msg *src);
void mb_msg_cp (struct mb_msg *dst, struct mb_msg *src);

#endif
