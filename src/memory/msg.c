#include "msg.h"
#include "chunk.h"

void mb_msg_init (struct mb_msg *self, size_t size)
{
    mb_chunkref_init (&self->sphdr, 0);
    mb_chunkref_init (&self->hdrs, 0);
    mb_chunkref_init (&self->body, size);
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
    mb_chunkref_term (&self->sphdr);
    mb_chunkref_term (&self->hdrs);
    mb_chunkref_term (&self->body);
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
