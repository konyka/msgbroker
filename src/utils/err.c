#include "err.h"
#include "../include/msgbroker/mb.h"
#include "fast.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#if defined _WIN32
#include "win.h"
#endif

static int mb_err_ctx = 0;

int mb_err_errno (void)
{
    return mb_err_ctx;
}

void mb_err_set_errno (int err)
{
    mb_err_ctx = err;
    errno = err;
}

static struct {
    int code;
    const char *msg;
} mb_err_native[] = {
    { ENOTSUP,          "Operation not supported" },
    { EPROTONOSUPPORT,  "Protocol not supported" },
    { ENOBUFS,          "No buffer space available" },
    { ENETDOWN,         "Network is down" },
    { EADDRINUSE,       "Address in use" },
    { EADDRNOTAVAIL,    "Address not available" },
    { ECONNREFUSED,     "Connection refused" },
    { EINPROGRESS,      "Operation in progress" },
    { ENOTSOCK,         "Not a socket" },
    { EAFNOSUPPORT,     "Address family not supported" },
    { EPROTO,           "Protocol error" },
    { EAGAIN,           "Resource temporarily unavailable" },
    { EBADF,            "Bad file descriptor" },
    { EINVAL,           "Invalid argument" },
    { EMFILE,           "Too many open files" },
    { EFAULT,           "Bad address" },
    { EACCES,           "Permission denied" },
    { ENETRESET,        "Network dropped connection on reset" },
    { ENETUNREACH,      "Network is unreachable" },
    { EHOSTUNREACH,     "No route to host" },
    { ENOTCONN,         "Transport endpoint is not connected" },
    { EMSGSIZE,         "Message too long" },
    { ETIMEDOUT,        "Connection timed out" },
    { ECONNABORTED,     "Software caused connection abort" },
    { ECONNRESET,       "Connection reset by peer" },
    { ENOPROTOOPT,      "Protocol not available" },
    { EISCONN,          "Transport endpoint is already connected" },
    { ESOCKTNOSUPPORT,  "Socket type not supported" },
    { ETERM,            "Context was terminated" },
    { EFSM,             "Operation cannot be performed in current state" },
    { EOPNOTSUPP,       "Operation not supported on transport endpoint" },
    { ENOMEM,           "Cannot allocate memory" },
    { 0, NULL },
};

const char *mb_err_strerror (int errnum)
{
    int i;
    for (i = 0; mb_err_native[i].msg != NULL; i++) {
        if (mb_err_native[i].code == errnum)
            return mb_err_native[i].msg;
    }
    return strerror (errnum);
}
