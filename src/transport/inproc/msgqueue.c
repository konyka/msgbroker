#include "msgqueue.h"
#include "../../utils/alloc.h"

#include <string.h>
#include <errno.h>

void mb_msgqueue_init (struct mb_msgqueue *self, size_t maxmem)
{
    self->in.chunk = NULL;
    self->in.pos = 0;
    self->out.chunk = NULL;
    self->out.pos = 0;
    self->count = 0;
    self->mem = 0;
    self->maxmem = maxmem;
    self->cache = NULL;
    mb_mutex_init (&self->sync);
}

void mb_msgqueue_term (struct mb_msgqueue *self)
{
    struct mb_msgqueue_chunk *chunk;
    struct mb_msgqueue_chunk *next;
    int i;

    mb_mutex_lock (&self->sync);

    if (self->cache) {
        mb_free (self->cache);
        self->cache = NULL;
    }

    chunk = self->in.chunk;
    while (chunk) {
        next = chunk->next;
        for (i = 0; i < MB_MSGQUEUE_GRANULARITY; ++i)
            mb_msg_term (&chunk->msgs[i]);
        mb_free (chunk);
        chunk = next;
    }
    self->in.chunk = NULL;
    self->out.chunk = NULL;
    self->count = 0;

    mb_mutex_unlock (&self->sync);
    mb_mutex_term (&self->sync);
}

int mb_msgqueue_empty (struct mb_msgqueue *self)
{
    int empty;

    mb_mutex_lock (&self->sync);
    empty = self->count == 0;
    mb_mutex_unlock (&self->sync);
    return empty;
}

int mb_msgqueue_push (struct mb_msgqueue *self, struct mb_msg *msg)
{
    struct mb_msgqueue_chunk *chunk;
    int was_empty;

    mb_mutex_lock (&self->sync);

    if (self->maxmem > 0 && self->mem >= self->maxmem) {
        mb_mutex_unlock (&self->sync);
        return -EAGAIN;
    }

    was_empty = (self->count == 0);

    if (!self->out.chunk) {
        if (self->cache) {
            chunk = self->cache;
            self->cache = NULL;
        } else {
            chunk = (struct mb_msgqueue_chunk *)
                mb_alloc (sizeof (struct mb_msgqueue_chunk));
            if (!chunk) {
                mb_mutex_unlock (&self->sync);
                return -ENOMEM;
            }
            memset (chunk->msgs, 0, sizeof (chunk->msgs));
        }
        chunk->next = NULL;
        self->out.chunk = chunk;
        self->out.pos = 0;
        if (!self->in.chunk) {
            self->in.chunk = chunk;
            self->in.pos = 0;
        }
    }

    mb_msg_mv (&self->out.chunk->msgs[self->out.pos], msg);
    ++self->out.pos;
    ++self->count;
    self->mem += mb_chunkref_size (&self->out.chunk->msgs[self->out.pos - 1].body);

    if (self->out.pos == MB_MSGQUEUE_GRANULARITY) {
        if (self->cache) {
            chunk = self->cache;
            self->cache = NULL;
        } else {
            chunk = (struct mb_msgqueue_chunk *)
                mb_alloc (sizeof (struct mb_msgqueue_chunk));
            if (!chunk) {
                self->out.pos = MB_MSGQUEUE_GRANULARITY;
                mb_mutex_unlock (&self->sync);
                return was_empty ? 1 : 0;
            }
            memset (chunk->msgs, 0, sizeof (chunk->msgs));
        }
        chunk->next = NULL;
        self->out.chunk->next = chunk;
        self->out.chunk = chunk;
        self->out.pos = 0;
    }

    mb_mutex_unlock (&self->sync);
    return was_empty ? 1 : 0;
}

void mb_msgqueue_pop (struct mb_msgqueue *self, struct mb_msg *msg)
{
    struct mb_msgqueue_chunk *chunk;

    mb_mutex_lock (&self->sync);

    if (self->count == 0) {
        mb_mutex_unlock (&self->sync);
        return;
    }

    mb_msg_mv (msg, &self->in.chunk->msgs[self->in.pos]);
    --self->count;
    self->mem -= mb_chunkref_size (&msg->body);
    ++self->in.pos;

    if (self->in.pos == MB_MSGQUEUE_GRANULARITY ||
        (self->in.chunk != self->out.chunk && self->in.pos == self->out.pos)) {
        chunk = self->in.chunk;
        self->in.chunk = chunk->next;
        self->in.pos = 0;
        if (self->cache)
            mb_free (chunk);
        else
            self->cache = chunk;
    }

    mb_mutex_unlock (&self->sync);
}
