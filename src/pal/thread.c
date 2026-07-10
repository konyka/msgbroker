#include "thread.h"
#include "../utils/alloc.h"

#include <errno.h>

#if defined _WIN32
#include "win.h"
#else
#include <pthread.h>
#endif

struct mb_thread_wrapper_args {
    mb_thread_fn routine;
    void *arg;
};

#if defined _WIN32
static DWORD WINAPI mb_thread_wrapper (LPVOID arg)
{
    struct mb_thread_wrapper_args *args = (struct mb_thread_wrapper_args *) arg;
    mb_thread_fn fn = args->routine;
    void *fn_arg = args->arg;
    mb_free (args);
    fn (fn_arg);
    return 0;
}
#else
static void *mb_thread_wrapper (void *arg)
{
    struct mb_thread_wrapper_args *args = (struct mb_thread_wrapper_args *) arg;
    mb_thread_fn fn = args->routine;
    void *fn_arg = args->arg;
    mb_free (args);
    fn (fn_arg);
    return NULL;
}
#endif

void mb_thread_init (struct mb_thread *self)
{
#if defined _WIN32
    self->handle = NULL;
#else
    self->handle = (pthread_t) 0;
#endif
    self->routine = NULL;
    self->arg = NULL;
}

void mb_thread_term (struct mb_thread *self)
{
    /* Join if still running; safe no-op when already joined. */
    (void) mb_thread_join (self);
}

int mb_thread_start (struct mb_thread *self, mb_thread_fn routine, void *arg)
{
    struct mb_thread_wrapper_args *args;
    args = (struct mb_thread_wrapper_args *) mb_alloc (sizeof (*args));
    if (!args) return -ENOMEM;
    args->routine = routine;
    args->arg = arg;

#if defined _WIN32
    self->handle = CreateThread (NULL, 0, mb_thread_wrapper, args, 0, NULL);
    if (!self->handle) {
        mb_free (args);
        return -EFAULT;
    }
    return 0;
#else
    int rc = pthread_create (&self->handle, NULL, mb_thread_wrapper, args);
    if (rc != 0) {
        mb_free (args);
        return -rc;
    }
    return 0;
#endif
}

int mb_thread_join (struct mb_thread *self)
{
#if defined _WIN32
    if (!self->handle)
        return 0;
    WaitForSingleObject (self->handle, INFINITE);
    CloseHandle (self->handle);
    self->handle = NULL;
    return 0;
#else
    int rc;

    if (!self->handle)
        return 0;
    rc = pthread_join (self->handle, NULL);
    self->handle = (pthread_t) 0;
    return rc;
#endif
}
