#ifndef MB_CONDVAR_H_INCLUDED
#define MB_CONDVAR_H_INCLUDED

#if defined _WIN32
#include "win.h"
#else
#include <pthread.h>
#endif

#include "mutex.h"

struct mb_condvar {
#if defined _WIN32
    CONDITION_VARIABLE cv;
#else
    pthread_cond_t cond;
#endif
};

void mb_condvar_init (struct mb_condvar *self);
void mb_condvar_term (struct mb_condvar *self);
void mb_condvar_signal (struct mb_condvar *self);
void mb_condvar_broadcast (struct mb_condvar *self);
int mb_condvar_wait (struct mb_condvar *self, struct mb_mutex *mutex, int timeout_ms);

#endif
