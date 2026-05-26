/*
    msgbroker -- High-performance messaging library in pure C.

    Copyright 2024 msgbroker contributors. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef MB_H_INCLUDED
#define MB_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*  Handle DSO symbol visibility. */
#if !defined(MB_EXPORT)
#    if defined(_WIN32) && !defined(MB_STATIC_LIB)
#        if defined MB_SHARED_LIB
#            define MB_EXPORT __declspec(dllexport)
#        else
#            define MB_EXPORT __declspec(dllimport)
#        endif
#    else
#        define MB_EXPORT extern
#    endif
#endif

/******************************************************************************/
/*  Versioning.                                                               */
/******************************************************************************/

#define MB_VERSION_MAJOR 0
#define MB_VERSION_MINOR 2
#define MB_VERSION_PATCH 0

#define MB_VERSION_STRING "0.2.0"

/******************************************************************************/
/*  Limits.                                                                   */
/******************************************************************************/

#ifndef MB_MAX_SOCKETS
#define MB_MAX_SOCKETS 1024
#endif

#define MB_SOCKADDR_MAX 128

/******************************************************************************/
/*  Errors.                                                                   */
/******************************************************************************/

/*  A number random enough not to collide with different errno ranges on
    different OSes. The assumption is that error_t is at least 32-bit type. */
#define MB_HAUSNUMERO 156384712

/*  On some platforms some standard POSIX errnos are not defined. */
#ifndef ENOTSUP
#define ENOTSUP (MB_HAUSNUMERO + 1)
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT (MB_HAUSNUMERO + 2)
#endif
#ifndef ENOBUFS
#define ENOBUFS (MB_HAUSNUMERO + 3)
#endif
#ifndef ENETDOWN
#define ENETDOWN (MB_HAUSNUMERO + 4)
#endif
#ifndef EADDRINUSE
#define EADDRINUSE (MB_HAUSNUMERO + 5)
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL (MB_HAUSNUMERO + 6)
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED (MB_HAUSNUMERO + 7)
#endif
#ifndef EINPROGRESS
#define EINPROGRESS (MB_HAUSNUMERO + 8)
#endif
#ifndef ENOTSOCK
#define ENOTSOCK (MB_HAUSNUMERO + 9)
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT (MB_HAUSNUMERO + 10)
#endif
#ifndef EPROTO
#define EPROTO (MB_HAUSNUMERO + 11)
#endif
#ifndef EAGAIN
#define EAGAIN (MB_HAUSNUMERO + 12)
#endif
#ifndef EBADF
#define EBADF (MB_HAUSNUMERO + 13)
#endif
#ifndef EINVAL
#define EINVAL (MB_HAUSNUMERO + 14)
#endif
#ifndef EMFILE
#define EMFILE (MB_HAUSNUMERO + 15)
#endif
#ifndef EFAULT
#define EFAULT (MB_HAUSNUMERO + 16)
#endif
#ifndef EACCES
#define EACCES (MB_HAUSNUMERO + 17)
#endif
#ifndef EACCESS
#define EACCESS (EACCES)
#endif
#ifndef ENETRESET
#define ENETRESET (MB_HAUSNUMERO + 18)
#endif
#ifndef ENETUNREACH
#define ENETUNREACH (MB_HAUSNUMERO + 19)
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH (MB_HAUSNUMERO + 20)
#endif
#ifndef ENOTCONN
#define ENOTCONN (MB_HAUSNUMERO + 21)
#endif
#ifndef EMSGSIZE
#define EMSGSIZE (MB_HAUSNUMERO + 22)
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT (MB_HAUSNUMERO + 23)
#endif
#ifndef ECONNABORTED
#define ECONNABORTED (MB_HAUSNUMERO + 24)
#endif
#ifndef ECONNRESET
#define ECONNRESET (MB_HAUSNUMERO + 25)
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT (MB_HAUSNUMERO + 26)
#endif
#ifndef EISCONN
#define EISCONN (MB_HAUSNUMERO + 27)
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT (MB_HAUSNUMERO + 28)
#endif

/*  Native msgbroker error codes. */
#ifndef ETERM
#define ETERM (MB_HAUSNUMERO + 53)
#endif
#ifndef EFSM
#define EFSM (MB_HAUSNUMERO + 54)
#endif

/** @brief Get the thread-safe errno value for MB operations. */
MB_EXPORT int mb_errno (void);
/** @brief Get a human-readable string for an MB error number. */
MB_EXPORT const char *mb_strerror (int errnum);

/******************************************************************************/
/*  Address families.                                                         */
/******************************************************************************/

#define AF_MB 1
#define AF_MB_RAW 2

/******************************************************************************/
/*  Socket option levels.                                                     */
/******************************************************************************/

/*  Negative numbers reserved for transports, positive for socket types. */
#define MB_SOL_SOCKET 0

/*  Generic socket options (MB_SOL_SOCKET level). */
#define MB_LINGER              1
#define MB_SNDBUF              2
#define MB_RCVBUF              3
#define MB_SNDTIMEO            4
#define MB_RCVTIMEO            5
#define MB_RECONNECT_IVL       6
#define MB_RECONNECT_IVL_MAX   7
#define MB_SNDPRIO             8
#define MB_RCVPRIO             9
#define MB_SNDFD              10
#define MB_RCVFD              11
#define MB_DOMAIN             12
#define MB_PROTOCOL           13
#define MB_IPV4ONLY           14
#define MB_SOCKET_NAME        15
#define MB_RCVMAXSIZE         16
#define MB_MAXTTL             17

/*  Send/recv flags. */
#define MB_DONTWAIT 1

/******************************************************************************/
/*  Zero-copy support.                                                        */
/******************************************************************************/

#define MB_MSG ((size_t) -1)

/** @brief Allocate a new zero-copy message buffer. */
MB_EXPORT void *mb_allocmsg (size_t size);
/** @brief Reallocate an existing message buffer. */
MB_EXPORT void *mb_reallocmsg (void *msg, size_t size);
/** @brief Free a previously allocated message buffer. */
MB_EXPORT int mb_freemsg (void *msg);

/******************************************************************************/
/*  Message I/O structures.                                                   */
/******************************************************************************/

struct mb_iovec {
    void *iov_base;
    size_t iov_len;
};

struct mb_msghdr {
    struct mb_iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
};

struct mb_cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

/*  Internal. */
/** @brief Get next control message header for a msghdr. */
MB_EXPORT struct mb_cmsghdr *mb_cmsg_nxthdr_ (
    const struct mb_msghdr *mhdr,
    const struct mb_cmsghdr *cmsg);

#define MB_CMSG_ALIGN_(len) \
    (((len) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1))

#define MB_CMSG_FIRSTHDR(mhdr) \
    mb_cmsg_nxthdr_ ((struct mb_msghdr*) (mhdr), NULL)

#define MB_CMSG_NXTHDR(mhdr, cmsg) \
    mb_cmsg_nxthdr_ ((struct mb_msghdr*) (mhdr), (struct mb_cmsghdr*) (cmsg))

#define MB_CMSG_DATA(cmsg) \
    ((unsigned char*) (((struct mb_cmsghdr*) (cmsg)) + 1))

#define MB_CMSG_SPACE(len) \
    (MB_CMSG_ALIGN_ (len) + MB_CMSG_ALIGN_ (sizeof (struct mb_cmsghdr)))

#define MB_CMSG_LEN(len) \
    (MB_CMSG_ALIGN_ (sizeof (struct mb_cmsghdr)) + (len))

/*  SP header constants. */
#define PROTO_SP 1
#define SP_HDR 1

/******************************************************************************/
/*  Socket lifecycle.                                                         */
/******************************************************************************/

/** @brief Create a new socket for the given domain and protocol. */
MB_EXPORT int mb_socket (int domain, int protocol);
/** @brief Close a previously opened socket descriptor. */
MB_EXPORT int mb_close (int s);
/** @brief Set socket options at a given level. */
MB_EXPORT int mb_setsockopt (int s, int level, int option,
    const void *optval, size_t optvallen);
/** @brief Get socket options for a socket. */
MB_EXPORT int mb_getsockopt (int s, int level, int option,
    void *optval, size_t *optvallen);
/** @brief Bind a socket to a local address. */
MB_EXPORT int mb_bind (int s, const char *addr);
/** @brief Connect a socket to a remote address. */
MB_EXPORT int mb_connect (int s, const char *addr);
/** @brief Shutdown a socket's send/receive directions. */
MB_EXPORT int mb_shutdown (int s, int how);

/******************************************************************************/
/*  Messaging.                                                                */
/******************************************************************************/

/** @brief Send data on a socket. */
MB_EXPORT int mb_send (int s, const void *buf, size_t len, int flags);
/** @brief Receive data from a socket. */
MB_EXPORT int mb_recv (int s, void *buf, size_t len, int flags);
/** @brief Send a message using a iovec/msghdr structure. */
MB_EXPORT int mb_sendmsg (int s, const struct mb_msghdr *msghdr, int flags);
/** @brief Receive a message into a iovec/msghdr structure. */
MB_EXPORT int mb_recvmsg (int s, struct mb_msghdr *msghdr, int flags);

/******************************************************************************/
/*  Socket multiplexing.                                                      */
/******************************************************************************/

#define MB_POLLIN  1
#define MB_POLLOUT 2

struct mb_pollfd {
    int fd;
    short events;
    short revents;
};

MB_EXPORT int mb_poll (struct mb_pollfd *fds, int nfds, int timeout);

/******************************************************************************/
/*  Device forwarding.                                                        */
/******************************************************************************/

MB_EXPORT int mb_device (int s1, int s2);

/******************************************************************************/
/*  Statistics.                                                               */
/******************************************************************************/

/*  Transport statistics. */
#define MB_STAT_ESTABLISHED_CONNECTIONS  101
#define MB_STAT_ACCEPTED_CONNECTIONS     102
#define MB_STAT_DROPPED_CONNECTIONS      103
#define MB_STAT_BROKEN_CONNECTIONS       104
#define MB_STAT_CONNECT_ERRORS           105
#define MB_STAT_BIND_ERRORS              106
#define MB_STAT_ACCEPT_ERRORS            107

/*  Current state statistics. */
#define MB_STAT_CURRENT_CONNECTIONS      201
#define MB_STAT_INPROGRESS_CONNECTIONS   202
#define MB_STAT_CURRENT_EP_ERRORS        203

/*  Message statistics. */
#define MB_STAT_MESSAGES_SENT            301
#define MB_STAT_MESSAGES_RECEIVED        302
#define MB_STAT_BYTES_SENT               303
#define MB_STAT_BYTES_RECEIVED           304

/*  Protocol statistics. */
#define MB_STAT_CURRENT_SND_PRIORITY     401

MB_EXPORT uint64_t mb_get_statistic (int s, int stat);

/******************************************************************************/
/*  Library lifecycle.                                                        */
/******************************************************************************/

MB_EXPORT void mb_term (void);

/******************************************************************************/
/*  Version information.                                                      */
/******************************************************************************/

MB_EXPORT int mb_version_major (void);
MB_EXPORT int mb_version_minor (void);
MB_EXPORT int mb_version_patch (void);
MB_EXPORT const char *mb_version_string (void);

/******************************************************************************/
/*  Coroutine-friendly I/O.                                                   */
/******************************************************************************/

MB_EXPORT int mb_coro_send (int s, const void *buf, size_t len);
MB_EXPORT int mb_coro_recv (int s, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MB_H_INCLUDED */
