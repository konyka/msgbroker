#ifndef MB_CORE_LIMITS_H_INCLUDED
#define MB_CORE_LIMITS_H_INCLUDED

#include <stddef.h>

/*  T-LIMITS: process-wide security caps installed by mb_limits_install_defaults
    on first mb_socket. mb_sock_setopt consults mb_limits_check_* to reject
    values above the active cap with -EPERM; mb_limits_set_* is called for the
    MB_LIMITS_* setters themselves. */

#define MB_LIMITS_DEFAULT_RCVBUF    (16 * 1024 * 1024)  /* 16 MiB    */
#define MB_LIMITS_DEFAULT_TIMEO_MS  (60 * 1000)         /* 60 s      */
#define MB_LIMITS_DEFAULT_BACKLOG   128

void mb_limits_install_defaults (void);
int  mb_limits_get_rcvbuf (void);
int  mb_limits_get_sndtimeo (void);
int  mb_limits_get_rcvtimeo (void);
int  mb_limits_get_backlog (void);

int mb_limits_set_rcvbuf (const void *optval, size_t optvallen);
int mb_limits_set_sndtimeo (const void *optval, size_t optvallen);
int mb_limits_set_rcvtimeo (const void *optval, size_t optvallen);
int mb_limits_set_backlog (const void *optval, size_t optvallen);

int mb_limits_check_rcvbuf (int requested);
int mb_limits_check_sndtimeo (int requested);
int mb_limits_check_rcvtimeo (int requested);
int mb_limits_check_backlog (int requested);

#endif
