#ifndef MB_QUEUE_H_INCLUDED
#define MB_QUEUE_H_INCLUDED

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

/*  MPSC (multi-producer single-consumer) lock-free queue. */
struct mb_mpsc_queue_item {
    struct mb_mpsc_queue_item *next;
};

struct mb_mpsc_queue {
    struct mb_mpsc_queue_item *head;
    struct mb_mpsc_queue_item *stub;
    void *producer_tail;
};

void mb_mpsc_queue_init (struct mb_mpsc_queue *self);
void mb_mpsc_queue_term (struct mb_mpsc_queue *self);
int mb_mpsc_queue_empty (struct mb_mpsc_queue *self);
void mb_mpsc_queue_push (struct mb_mpsc_queue *self,
    struct mb_mpsc_queue_item *item);
struct mb_mpsc_queue_item *mb_mpsc_queue_pop (struct mb_mpsc_queue *self);

#endif
