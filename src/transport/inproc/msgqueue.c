#include "msgqueue.h"
#include "../../utils/alloc.h"

#include <string.h>
#include <errno.h>

static int mb_msgqueue_fits_locked (const struct mb_msgqueue *self, size_t sz)
{
    if (self->maxmem == 0)
        return 1;
    return sz <= self->maxmem && self->mem <= self->maxmem - sz;
}

/* Empty bodies still need headroom when capped — matches can_push's fits(1). */
static size_t mb_msgqueue_check_sz (const struct mb_msgqueue *self, size_t sz)
{
    if (self->maxmem > 0 && sz == 0)
        return 1;
    return sz;
}

void mb_msgqueue_init (struct mb_msgqueue *self, size_t maxmem)
{
    self->in.chunk = NULL;
    self->in.pos = 0;
    self->out.chunk = NULL;
    self->out.pos = 0;
    self->count = 0;
    self->mem = 0;
    self->maxmem = maxmem;
    self->pending_sz = 0;
    self->cache = NULL;
    mb_mutex_init (&self->sync);
}

void mb_msgqueue_set_maxmem (struct mb_msgqueue *self, size_t maxmem)
{
    mb_mutex_lock (&self->sync);
    self->maxmem = maxmem;
    if (self->pending_sz && mb_msgqueue_fits_locked (self, self->pending_sz))
        self->pending_sz = 0;
    mb_mutex_unlock (&self->sync);
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
    self->pending_sz = 0;

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

int mb_msgqueue_can_push_sz (struct mb_msgqueue *self, size_t sz)
{
    int ok;

    mb_mutex_lock (&self->sync);
    ok = mb_msgqueue_fits_locked (self, mb_msgqueue_check_sz (self, sz));
    mb_mutex_unlock (&self->sync);
    return ok;
}

int mb_msgqueue_can_push (struct mb_msgqueue *self)
{
    int ok;

    mb_mutex_lock (&self->sync);
    if (self->pending_sz)
        ok = mb_msgqueue_fits_locked (self, self->pending_sz);
    else
        /* No failed size yet — report room for at least a 1-byte body. */
        ok = mb_msgqueue_fits_locked (self, 1);
    mb_mutex_unlock (&self->sync);
    return ok;
}

static struct mb_msgqueue_chunk *mb_msgqueue_alloc_chunk (struct mb_msgqueue *self)
{
    struct mb_msgqueue_chunk *chunk;

    if (self->cache) {
        chunk = self->cache;
        self->cache = NULL;
        return chunk;
    }
    chunk = (struct mb_msgqueue_chunk *)
        mb_alloc (sizeof (struct mb_msgqueue_chunk));
    if (!chunk)
        return NULL;
    memset (chunk->msgs, 0, sizeof (chunk->msgs));
    return chunk;
}

int mb_msgqueue_push (struct mb_msgqueue *self, struct mb_msg *msg)
{
    struct mb_msgqueue_chunk *chunk;
    int was_empty;
    size_t sz;

    mb_mutex_lock (&self->sync);

    sz = mb_chunkref_size (&msg->body);
    /* Cap includes this message; avoid overflow and single oversized push.
     * Zero-size bodies still require fits(1) headroom so a full queue cannot
     * accept unbounded empty messages while can_push stays false. */
    {
        size_t check_sz = mb_msgqueue_check_sz (self, sz);
        if (!mb_msgqueue_fits_locked (self, check_sz)) {
            if (self->maxmem > 0)
                self->pending_sz = check_sz;
            mb_mutex_unlock (&self->sync);
            return -EAGAIN;
        }
    }

    was_empty = (self->count == 0);

    /* Need a writable slot: first chunk, or after a full chunk when the
     * previous eager alloc failed (pos left at GRANULARITY). */
    if (!self->out.chunk || self->out.pos == MB_MSGQUEUE_GRANULARITY) {
        chunk = mb_msgqueue_alloc_chunk (self);
        if (!chunk) {
            mb_mutex_unlock (&self->sync);
            return -ENOMEM;
        }
        chunk->next = NULL;
        if (!self->out.chunk) {
            self->out.chunk = chunk;
            if (!self->in.chunk) {
                self->in.chunk = chunk;
                self->in.pos = 0;
            }
        } else {
            self->out.chunk->next = chunk;
            self->out.chunk = chunk;
        }
        self->out.pos = 0;
    }

    mb_msg_mv (&self->out.chunk->msgs[self->out.pos], msg);
    ++self->out.pos;
    ++self->count;
    self->mem += sz;

    if (self->pending_sz && mb_msgqueue_fits_locked (self, self->pending_sz))
        self->pending_sz = 0;

    /* Eagerly prepare the next chunk; failure is OK — next push allocates. */
    if (self->out.pos == MB_MSGQUEUE_GRANULARITY) {
        chunk = mb_msgqueue_alloc_chunk (self);
        if (chunk) {
            chunk->next = NULL;
            self->out.chunk->next = chunk;
            self->out.chunk = chunk;
            self->out.pos = 0;
        }
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

    /* Advance past a fully consumed chunk:
     * - intermediate chunk: always GRANULARITY slots
     * - active out chunk: stop at the write cursor (out.pos)
     * Never compare in.pos to out.pos across different chunks. */
    if (self->in.pos == MB_MSGQUEUE_GRANULARITY ||
        (self->in.chunk == self->out.chunk &&
         self->in.pos == self->out.pos)) {
        chunk = self->in.chunk;
        self->in.chunk = chunk->next;
        self->in.pos = 0;
        if (chunk == self->out.chunk) {
            self->out.chunk = NULL;
            self->out.pos = 0;
        }
        if (self->cache)
            mb_free (chunk);
        else
            self->cache = chunk;
    }

    if (self->pending_sz && mb_msgqueue_fits_locked (self, self->pending_sz))
        self->pending_sz = 0;

    mb_mutex_unlock (&self->sync);
}
