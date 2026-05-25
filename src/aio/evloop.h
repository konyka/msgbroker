#ifndef MB_EVLOOP_H_INCLUDED
#define MB_EVLOOP_H_INCLUDED

#if defined _WIN32
#include "../pal/win.h"
#elif defined __linux__
#include <sys/epoll.h>
#include <liburing.h>
#elif defined MB_HAVE_KQUEUE
#include <sys/event.h>
#endif

#include <stdint.h>

#define MB_EVLOOP_IN  1
#define MB_EVLOOP_OUT 2

#define MB_EVLOOP_BACKEND_EPOLL    1
#define MB_EVLOOP_BACKEND_IOURING  2

struct mb_evloop_cb {
    void (*on_event) (void *data, int events);
    void *data;
};

struct mb_evloop {
    int backend;

#if defined _WIN32
    HANDLE iocp;
#elif defined __linux__
    struct {
        int epoll_fd;
    } ep;
    struct {
        struct io_uring ring;
        int ring_fd;
    } iouring;
#elif defined MB_HAVE_KQUEUE
    int kqueue_fd;
#else
    int pollfd;
#endif
};

int mb_evloop_init (struct mb_evloop *self);
void mb_evloop_term (struct mb_evloop *self);
int mb_evloop_add (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb);
int mb_evloop_modify (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb);
int mb_evloop_remove (struct mb_evloop *self, int fd);
int mb_evloop_poll (struct mb_evloop *self, int timeout_ms);

const char *mb_evloop_backend_name (struct mb_evloop *self);

#endif
