#ifndef MB_MUTEX_H_INCLUDED
#define MB_MUTEX_H_INCLUDED

#if defined _WIN32
#include "win.h"
#else
#include <pthread.h>
#endif

struct mb_mutex {
#if defined _WIN32
    SRWLOCK srwlock;
#else
    pthread_mutex_t mutex;
#endif
};

void mb_mutex_init (struct mb_mutex *self);
void mb_mutex_term (struct mb_mutex *self);
void mb_mutex_lock (struct mb_mutex *self);
void mb_mutex_unlock (struct mb_mutex *self);

#endif
