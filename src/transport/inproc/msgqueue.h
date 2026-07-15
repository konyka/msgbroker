#ifndef MB_TRANSPORT_INPROC_MSGQUEUE_H_INCLUDED
#define MB_TRANSPORT_INPROC_MSGQUEUE_H_INCLUDED

#include "../../memory/msg.h"
#include "../../pal/mutex.h"

#include <stddef.h>

#define MB_MSGQUEUE_GRANULARITY 64

struct mb_msgqueue_chunk {
    struct mb_msg msgs[MB_MSGQUEUE_GRANULARITY];
    struct mb_msgqueue_chunk *next;
};

struct mb_msgqueue {
    struct {
        struct mb_msgqueue_chunk *chunk;
        int pos;
    } out;
    struct {
        struct mb_msgqueue_chunk *chunk;
        int pos;
    } in;
    size_t count;
    size_t mem;
    size_t maxmem;
    struct mb_msgqueue_chunk *cache;
    /* Protects concurrent peer push vs local pop (different sock ctx). */
    struct mb_mutex sync;
};

void mb_msgqueue_init (struct mb_msgqueue *self, size_t maxmem);
void mb_msgqueue_term (struct mb_msgqueue *self);
int mb_msgqueue_empty (struct mb_msgqueue *self);
/* Returns 1 if the queue was empty before the push, 0 if not, <0 on error. */
int mb_msgqueue_push (struct mb_msgqueue *self, struct mb_msg *msg);
void mb_msgqueue_pop (struct mb_msgqueue *self, struct mb_msg *msg);

#endif
