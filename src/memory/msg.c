#include "msg.h"
#include "chunk.h"
#include <string.h>

void mb_msg_init (struct mb_msg *self, size_t size)
{
    mb_chunkref_init (&self->sphdr, 0);
    mb_chunkref_init (&self->hdrs, 0);
    mb_chunkref_init (&self->body, size);
}

void mb_msg_init_data (struct mb_msg *self, const void *data, size_t size)
{
    mb_chunkref_init (&self->sphdr, 0);
    mb_chunkref_init (&self->hdrs, 0);
    mb_chunkref_init (&self->body, size);
    if (size > 0)
        memcpy (mb_chunkref_data (&self->body), data, size);
}

void mb_msg_init_chunk (struct mb_msg *self, void *chunk)
{
    mb_chunkref_init (&self->sphdr, 0);
    mb_chunkref_init (&self->hdrs, 0);
    mb_chunkref_init_chunk (&self->body, chunk, 0, mb_chunk_size (chunk));
    mb_chunk_free (chunk);
}

void mb_msg_term (struct mb_msg *self)
{
    if (self->body.size > MB_CHUNKREF_MAX && self->body.u.chunk)
        mb_chunk_free (self->body.u.chunk);
    if (self->hdrs.size > MB_CHUNKREF_MAX && self->hdrs.u.chunk)
        mb_chunk_free (self->hdrs.u.chunk);
    if (self->sphdr.size > MB_CHUNKREF_MAX && self->sphdr.u.chunk)
        mb_chunk_free (self->sphdr.u.chunk);
    self->sphdr.size = 0; self->sphdr.u.chunk = NULL;
    self->hdrs.size = 0;   self->hdrs.u.chunk = NULL;
    self->body.size = 0;   self->body.u.chunk = NULL;
}

void mb_msg_mv (struct mb_msg *dst, struct mb_msg *src)
{
    mb_chunkref_mv (&dst->sphdr, &src->sphdr);
    mb_chunkref_mv (&dst->hdrs, &src->hdrs);
    mb_chunkref_mv (&dst->body, &src->body);
}

void mb_msg_cp (struct mb_msg *dst, struct mb_msg *src)
{
    mb_chunkref_cp (&dst->sphdr, &src->sphdr);
    mb_chunkref_cp (&dst->hdrs, &src->hdrs);
    mb_chunkref_cp (&dst->body, &src->body);
}
