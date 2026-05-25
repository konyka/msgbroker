#ifndef MB_EFD_H_INCLUDED
#define MB_EFD_H_INCLUDED

#if defined _WIN32
#include "win.h"
#endif

struct mb_efd {
#if defined _WIN32
    HANDLE event;
#elif defined MB_HAVE_EVENTFD
    int fd;
#else
    int fds [2];
#endif
};

void mb_efd_init (struct mb_efd *self);
void mb_efd_term (struct mb_efd *self);
int mb_efd_getfd (struct mb_efd *self);
void mb_efd_signal (struct mb_efd *self);
void mb_efd_unsignal (struct mb_efd *self);
int mb_efd_wait (struct mb_efd *self, int timeout_ms);

#endif
