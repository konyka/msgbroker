#include "pool.h"
#include "msg.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Pool-owned slab+arena layout for mb_mempool_alloc_msg / mb_mempool_free_msg.
 *
 * The msg slab caps at 1024 entries; past that, allocations fall through to
 * the arena the pool already owns.  Each arena-served chunk is prefixed with
 * an 8-byte tag (magic + obj_size echo) so mb_mempool_free_msg can tell it
 * apart from a slab-owned chunk and route it back to the arena path instead
 * of double-feeding it to mb_slab_free (which would silently no-op today and
 * leak the chunk).  Arena memory is reclaimed wholesale by mb_arena_term, so
 * the free path is intentionally a no-op for arena-tagged chunks.
 */

#define MB_ARENA_MSG_MAGIC 0x41524D5347524F57u  /* "ARMSGROW" (LE) */
#define MB_ARENA_MSG_HDR   sizeof (uint64_t)

struct mb_arena_msg_hdr {
    uint64_t magic;
    uint64_t obj_size;
};

void mb_mempool_init (struct mb_mempool *self)
{
    mb_arena_init (&self->arena, 65536);
    mb_slab_init (&self->msg_slab, sizeof (struct mb_msg), 1024);
    mb_slab_init (&self->chunk_slab, 256, 2048);
}

void mb_mempool_term (struct mb_mempool *self)
{
    mb_slab_term (&self->chunk_slab);
    mb_slab_term (&self->msg_slab);
    mb_arena_term (&self->arena);
}

void *mb_mempool_alloc_msg (struct mb_mempool *self)
{
    void *slab_msg = mb_slab_alloc (&self->msg_slab);
    if (slab_msg)
        return slab_msg;

    /* Slab is full (or uninitialised). Fall through to the arena the pool
     * already owns, prefixing the user pointer with a tag so the free path
     * can route the free back to the arena instead of mb_slab_free. */
    {
        size_t msg_size = sizeof (struct mb_msg);
        size_t total = MB_ARENA_MSG_HDR + msg_size;
        struct mb_arena_msg_hdr *hdr;
        void *raw;

        if (msg_size > SIZE_MAX - MB_ARENA_MSG_HDR)
            return NULL;
        raw = mb_arena_alloc (&self->arena, total);
        if (!raw)
            return NULL;
        hdr = (struct mb_arena_msg_hdr *) raw;
        hdr->magic = MB_ARENA_MSG_MAGIC;
        hdr->obj_size = (uint64_t) msg_size;
        return (char *) raw + MB_ARENA_MSG_HDR;
    }
}

void mb_mempool_free_msg (struct mb_mempool *self, void *msg)
{
    struct mb_arena_msg_hdr *hdr;

    if (!msg)
        return;

    /* Arena-served chunk: header sits 8 bytes before the user pointer. */
    hdr = (struct mb_arena_msg_hdr *) ((char *) msg - MB_ARENA_MSG_HDR);
    if (hdr->magic == MB_ARENA_MSG_MAGIC &&
        hdr->obj_size == (uint64_t) sizeof (struct mb_msg)) {
        /* Arena memory is reclaimed wholesale by mb_arena_term; explicit
         * per-object free would require arena tracking that doesn't exist
         * today, so we leak-by-design into the arena reset path. */
        hdr->magic = 0;
        hdr->obj_size = 0;
        return;
    }

    mb_slab_free (&self->msg_slab, msg);
}

void *mb_mempool_alloc (struct mb_mempool *self, size_t size)
{
    /* Arena-only: heap fallback would leak on mb_mempool_term. */
    return mb_arena_alloc (&self->arena, size);
}