#ifndef MB_QUEUE_H_INCLUDED
#define MB_QUEUE_H_INCLUDED

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define MB_HAVE_C11_ATOMICS 1
#else
#define MB_HAVE_C11_ATOMICS 0
#endif

struct mb_queue_item {
    struct mb_queue_item *next;
};

struct mb_queue {
    struct mb_queue_item *head;
    struct mb_queue_item *tail;
};

#define MB_QUEUE_NOTINQUEUE ((struct mb_queue_item *) -1)
#define MB_QUEUE_ITEM_INITIALIZER {MB_QUEUE_NOTINQUEUE}

void mb_queue_init (struct mb_queue *self);
void mb_queue_term (struct mb_queue *self);
int mb_queue_empty (struct mb_queue *self);
void mb_queue_push (struct mb_queue *self, struct mb_queue_item *item);
void mb_queue_remove (struct mb_queue *self, struct mb_queue_item *item);
struct mb_queue_item *mb_queue_pop (struct mb_queue *self);
void mb_queue_item_init (struct mb_queue_item *self);
void mb_queue_item_term (struct mb_queue_item *self);
int mb_queue_item_isinqueue (struct mb_queue_item *self);

/*  MPSC (multi-producer single-consumer) lock-free queue.
 *
 *  `next` on the item and `producer_tail` on the queue are touched
 *  by both producers and the consumer, so both must be C11 atomic
 *  when stdatomic.h is available.  The lock-free Vyukov algorithm
 *  uses a CAS on producer_tail (release on success, acquire on
 *  failure) and a release-store on prev->next.  The consumer
 *  acquire-loads both producer_tail and head->next. */
struct mb_mpsc_queue_item {
#if MB_HAVE_C11_ATOMICS
    _Atomic (struct mb_mpsc_queue_item *) next;
#else
    struct mb_mpsc_queue_item *next;
#endif
};

struct mb_mpsc_queue {
    struct mb_mpsc_queue_item *head;
    struct mb_mpsc_queue_item *stub;
#if MB_HAVE_C11_ATOMICS
    _Atomic (struct mb_mpsc_queue_item *) producer_tail;
#else
    void *producer_tail;
#endif
};

void mb_mpsc_queue_init (struct mb_mpsc_queue *self);
void mb_mpsc_queue_term (struct mb_mpsc_queue *self);
int mb_mpsc_queue_empty (struct mb_mpsc_queue *self);
void mb_mpsc_queue_push (struct mb_mpsc_queue *self,
    struct mb_mpsc_queue_item *item);
struct mb_mpsc_queue_item *mb_mpsc_queue_pop (struct mb_mpsc_queue *self);

#endif
