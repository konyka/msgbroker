#include "chunk.h"
#include "../utils/alloc.h"
#include "../pal/atomic.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct mb_chunk_hdr {
    uint32_t refcount;
    size_t size;
    size_t offset;
    char data [];
};

#define MB_CHUNK_HDR_SIZE (sizeof (struct mb_chunk_hdr))
#define MB_CHUNK_FROM_DATA(p) \
    ((struct mb_chunk_hdr *) ((char *) (p) - MB_CHUNK_HDR_SIZE))
#define MB_CHUNK_DATA(h) ((void *) ((h)->data + (h)->offset))

int mb_chunk_alloc (size_t size, void **result)
{
    struct mb_chunk_hdr *hdr;

    if (size > SIZE_MAX - MB_CHUNK_HDR_SIZE)
        return -ENOMEM;

    hdr = (struct mb_chunk_hdr *) mb_alloc (MB_CHUNK_HDR_SIZE + size);
    if (!hdr)
        return -ENOMEM;
    hdr->refcount = 1;
    hdr->size = size;
    hdr->offset = 0;
    *result = hdr->data;
    return 0;
}

int mb_chunk_realloc (size_t size, void **chunk)
{
    struct mb_chunk_hdr *hdr = MB_CHUNK_FROM_DATA (*chunk);
    size_t hdr_size;

    if (hdr->offset > SIZE_MAX - MB_CHUNK_HDR_SIZE)
        return -ENOMEM;
    hdr_size = MB_CHUNK_HDR_SIZE + hdr->offset;
    if (size > SIZE_MAX - hdr_size)
        return -ENOMEM;

    hdr = (struct mb_chunk_hdr *) mb_realloc (hdr, hdr_size + size);
    if (!hdr)
        return -ENOMEM;
    hdr->size = size;
    *chunk = hdr->data + hdr->offset;
    return 0;
}

void mb_chunk_free (void *p)
{
    struct mb_chunk_hdr *hdr = MB_CHUNK_FROM_DATA (p);
    if (__atomic_sub_fetch (&hdr->refcount, 1, __ATOMIC_SEQ_CST) == 0)
        mb_free (hdr);
}

void mb_chunk_addref (void *p, uint32_t n)
{
    struct mb_chunk_hdr *hdr = MB_CHUNK_FROM_DATA (p);
    __atomic_fetch_add (&hdr->refcount, n, __ATOMIC_SEQ_CST);
}

size_t mb_chunk_size (void *p)
{
    struct mb_chunk_hdr *hdr = MB_CHUNK_FROM_DATA (p);
    return hdr->size - hdr->offset;
}

void *mb_chunk_trim (void *p, size_t n)
{
    struct mb_chunk_hdr *hdr = MB_CHUNK_FROM_DATA (p);
    hdr->offset += n;
    return hdr->data + hdr->offset;
}
