#include "worker.h"
#include "../pal/clock.h"
#include "../utils/fast.h"

#include <stdlib.h>

int mb_worker_init (struct mb_worker *self)
{
    int rc;
    mb_thread_init (&self->thread);
    rc = mb_evloop_init (&self->evloop);
    if (rc != 0)
        return rc;
    mb_timerset_init (&self->timerset);
    mb_efd_init (&self->efd);
    mb_queue_init (&self->tasks);
    mb_mutex_init (&self->tasks_sync);
    self->running = 0;
    return 0;
}

void mb_worker_term (struct mb_worker *self)
{
    self->running = 0;
    mb_efd_signal (&self->efd);
    mb_thread_join (&self->thread);
    mb_mutex_term (&self->tasks_sync);
    mb_queue_term (&self->tasks);
    mb_efd_term (&self->efd);
    mb_timerset_term (&self->timerset);
    mb_evloop_term (&self->evloop);
    mb_thread_term (&self->thread);
}

void mb_worker_execute (struct mb_worker *self,
    struct mb_worker_task *task)
{
    mb_mutex_lock (&self->tasks_sync);
    mb_queue_push (&self->tasks, &task->item);
    mb_mutex_unlock (&self->tasks_sync);
    mb_efd_signal (&self->efd);
}

void mb_worker_task_init (struct mb_worker_task *self, int src,
    struct mb_fsm *owner)
{
    self->src = src;
    self->owner = owner;
    mb_queue_item_init (&self->item);
}

void mb_worker_task_term (struct mb_worker_task *self)
{
    mb_queue_item_term (&self->item);
}
