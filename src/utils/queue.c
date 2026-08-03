#include "queue.h"
#include "../pal/atomic.h"
#include "fast.h"
#include "alloc.h"

#include <stddef.h>
#include <stdlib.h>

void mb_queue_init (struct mb_queue *self)
{
    self->head = NULL;
    self->tail = NULL;
}

void mb_queue_term (struct mb_queue *self)
{
    self->head = NULL;
    self->tail = NULL;
}

int mb_queue_empty (struct mb_queue *self)
{
    return self->head == NULL;
}

void mb_queue_push (struct mb_queue *self, struct mb_queue_item *item)
{
    item->next = NULL;
    if (self->tail)
        self->tail->next = item;
    else
        self->head = item;
    self->tail = item;
}

void mb_queue_remove (struct mb_queue *self, struct mb_queue_item *item)
{
    struct mb_queue_item *it;
    struct mb_queue_item *prev;

    prev = NULL;
    for (it = self->head; it != NULL; it = it->next) {
        if (it == item) {
            if (prev)
                prev->next = it->next;
            else
                self->head = it->next;
            if (!it->next)
                self->tail = prev;
            item->next = MB_QUEUE_NOTINQUEUE;
            return;
        }
        prev = it;
    }
}

struct mb_queue_item *mb_queue_pop (struct mb_queue *self)
{
    struct mb_queue_item *result;
    if (!self->head)
        return NULL;
    result = self->head;
    self->head = result->next;
    if (!self->head)
        self->tail = NULL;
    result->next = MB_QUEUE_NOTINQUEUE;
    return result;
}

void mb_queue_item_init (struct mb_queue_item *self)
{
    self->next = MB_QUEUE_NOTINQUEUE;
}

void mb_queue_item_term (struct mb_queue_item *self)
{
    self->next = MB_QUEUE_NOTINQUEUE;
}

int mb_queue_item_isinqueue (struct mb_queue_item *self)
{
    return self->next != MB_QUEUE_NOTINQUEUE;
}

/* MPSC lock-free queue — Vyukov algorithm, C11 atomics edition.
 *
 *  Synchronization protocol (release/acquire, not seq_cst):
 *
 *    push (producer):
 *      1. load_acquire producer_tail -> prev   (synchronize with prior
 *         producer's CAS that stored prev)
 *      2. CAS_weak(prev -> item) on producer_tail with
 *         success=release / failure=acquire
 *         On success the release pairs with the consumer's or the
 *         next producer's acquire-load of producer_tail, making
 *         this producer's earlier stores visible.
 *      3. store_release prev->next = item
 *         Pairs with the acquire-load of next in pop, and with the
 *         acquire-load a competing producer performs on the item it
 *         sees as its prev.
 *
 *    pop (single consumer):
 *      load_acquire head->next
 *      load_acquire producer_tail
 *
 *  The legacy `__sync_bool_compare_and_swap` had implicit seq_cst on
 *  every operation.  C11 atomics let us drop down to acquire/release,
 *  which is sufficient for the Vyukov invariant and faster on
 *  weakly-ordered hardware (ARMv8, POWER). */
void mb_mpsc_queue_init (struct mb_mpsc_queue *self)
{
    self->stub = (struct mb_mpsc_queue_item *) mb_alloc (sizeof (*self->stub));
    if (!self->stub)
        abort ();
    atomic_store_explicit (&self->stub->next, NULL, memory_order_relaxed);
    self->head = self->stub;
    atomic_store_explicit (&self->producer_tail, self->stub,
        memory_order_relaxed);
}

void mb_mpsc_queue_term (struct mb_mpsc_queue *self)
{
    if (!self->stub)
        return;
    mb_free (self->stub);
    self->stub = NULL;
    self->head = NULL;
    atomic_store_explicit (&self->producer_tail, NULL, memory_order_relaxed);
}

int mb_mpsc_queue_empty (struct mb_mpsc_queue *self)
{
    return self->head == self->stub
        && atomic_load_explicit (&self->stub->next,
            memory_order_acquire) == NULL;
}

void mb_mpsc_queue_push (struct mb_mpsc_queue *self,
    struct mb_mpsc_queue_item *item)
{
    atomic_store_explicit (&item->next, NULL, memory_order_relaxed);
    struct mb_mpsc_queue_item *prev;
    do {
        prev = (struct mb_mpsc_queue_item *) atomic_load_explicit (
            &self->producer_tail, memory_order_acquire);
    } while (!atomic_compare_exchange_weak_explicit (
        &self->producer_tail, (struct mb_mpsc_queue_item **) &prev,
        item, memory_order_acq_rel, memory_order_acquire));
    atomic_store_explicit (&prev->next, item, memory_order_release);
}

struct mb_mpsc_queue_item *mb_mpsc_queue_pop (struct mb_mpsc_queue *self)
{
    struct mb_mpsc_queue_item *head = self->head;
    struct mb_mpsc_queue_item *next = (struct mb_mpsc_queue_item *)
        atomic_load_explicit (&head->next, memory_order_acquire);
    if (head == self->stub) {
        if (!next)
            return NULL;
        atomic_store_explicit (&self->stub->next, NULL,
            memory_order_relaxed);
        self->head = next;
        head = next;
        next = (struct mb_mpsc_queue_item *) atomic_load_explicit (
            &head->next, memory_order_acquire);
    }
    if (next) {
        self->head = next;
        return head;
    }
    /*  Last item in the chain.  If a producer is still in flight
     *  (its CAS hasn't completed yet), we have to wait for it
     *  to publish its `next` pointer. */
    if (atomic_load_explicit (&self->producer_tail,
            memory_order_acquire) != head)
        return NULL;
    /*  All producers are quiesced, but we still hold the stub as
     *  a sentinel.  Park the stub past our last real item so the
     *  chain remains linked. */
    mb_mpsc_queue_push (self, self->stub);
    next = (struct mb_mpsc_queue_item *) atomic_load_explicit (
        &head->next, memory_order_acquire);
    if (next) {
        self->head = next;
        return head;
    }
    return NULL;
}
