#ifndef MB_SEMAPHORE_H_INCLUDED
#define MB_SEMAPHORE_H_INCLUDED

#if defined _WIN32
#include "win.h"
#else
#include <semaphore.h>
#endif

struct mb_sem {
#if defined _WIN32
    HANDLE handle;
#else
    sem_t sem;
#endif
};

void mb_sem_init (struct mb_sem *self);
void mb_sem_term (struct mb_sem *self);
void mb_sem_post (struct mb_sem *self);
int mb_sem_wait (struct mb_sem *self);
int mb_sem_timedwait (struct mb_sem *self, int timeout_ms);

#endif
