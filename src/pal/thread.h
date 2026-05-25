#ifndef MB_THREAD_H_INCLUDED
#define MB_THREAD_H_INCLUDED

#if defined _WIN32
#include "win.h"
typedef DWORD mb_thread_routine;
#else
#include <pthread.h>
typedef void *(*mb_thread_routine_fn) (void *);
#endif

typedef void (*mb_thread_fn) (void *arg);

struct mb_thread {
#if defined _WIN32
    HANDLE handle;
#else
    pthread_t handle;
#endif
    mb_thread_fn routine;
    void *arg;
};

void mb_thread_init (struct mb_thread *self);
void mb_thread_term (struct mb_thread *self);
int mb_thread_start (struct mb_thread *self, mb_thread_fn routine, void *arg);
int mb_thread_join (struct mb_thread *self);

#endif
