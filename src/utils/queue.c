#include "queue.h"
#include "../pal/atomic.h"
#include "fast.h"

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

/* MPSC lock-free queue — Vyukov algorithm */
void mb_mpsc_queue_init (struct mb_mpsc_queue *self)
{
    self->stub = (struct mb_mpsc_queue_item *) malloc (sizeof (*self->stub));
    self->stub->next = NULL;
    self->head = self->stub;
    self->producer_tail = self->stub;
}

void mb_mpsc_queue_term (struct mb_mpsc_queue *self)
{
    free (self->stub);
    self->stub = NULL;
    self->head = NULL;
    self->producer_tail = NULL;
}

int mb_mpsc_queue_empty (struct mb_mpsc_queue *self)
{
    return self->head == self->stub && self->stub->next == NULL;
}

void mb_mpsc_queue_push (struct mb_mpsc_queue *self,
    struct mb_mpsc_queue_item *item)
{
    item->next = NULL;
    struct mb_mpsc_queue_item *prev;
    do {
        prev = (struct mb_mpsc_queue_item *) self->producer_tail;
    } while (!__sync_bool_compare_and_swap ((void *volatile *) &self->producer_tail,
        (void *) prev, (void *) item));
    prev->next = item;
}

struct mb_mpsc_queue_item *mb_mpsc_queue_pop (struct mb_mpsc_queue *self)
{
    struct mb_mpsc_queue_item *head = self->head;
    struct mb_mpsc_queue_item *next = head->next;
    if (head == self->stub) {
        if (!next)
            return NULL;
        self->stub->next = NULL;
        self->head = next;
        head = next;
        next = next->next;
    }
    if (next) {
        self->head = next;
        return head;
    }
    if (self->producer_tail != (void *) head)
        return NULL;
    mb_mpsc_queue_push (self, self->stub);
    next = head->next;
    if (next) {
        self->head = next;
        return head;
    }
    return NULL;
}
