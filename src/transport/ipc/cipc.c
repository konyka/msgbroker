#include "cipc.h"
#include "sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

static void mb_cipc_stop (void *p);
static void mb_cipc_destroy (void *p);

static const struct mb_ep_ops mb_cipc_ops = {
    mb_cipc_stop,
    mb_cipc_destroy,
};

static const char *mb_cipc_parse_addr (const char *addr, char *path,
    size_t pathlen)
{
    const char *sep;

    sep = strstr (addr, "://");
    if (!sep)
        return NULL;
    sep += 3;

    strncpy (path, sep, pathlen - 1);
    path[pathlen - 1] = '\0';
    return path;
}

int mb_cipc_create (struct mb_ep *ep)
{
    struct mb_cipc *self;
    struct sockaddr_un sa;
    int fd;
    int rc;
    char path[108];

    mb_cipc_parse_addr (mb_ep_getaddr (ep), path, sizeof (path));

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    memset (&sa, 0, sizeof (sa));
    sa.sun_family = AF_UNIX;
    snprintf (sa.sun_path, sizeof (sa.sun_path), "%s", path);

    rc = connect (fd, (struct sockaddr *) &sa, sizeof (sa));
    if (rc < 0) {
        close (fd);
        return -errno;
    }

    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

    self = (struct mb_cipc *) mb_alloc (sizeof (struct mb_cipc));
    if (!self) {
        close (fd);
        return -ENOMEM;
    }

    self->ep = ep;
    self->sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
    if (!self->sipc) {
        close (fd);
        mb_free (self);
        return -ENOMEM;
    }

    mb_sipc_create (self->sipc, ep, fd);
    mb_sipc_start (self->sipc);

    mb_ep_tran_setup (ep, &mb_cipc_ops, self);

    return 0;
}

static void mb_cipc_stop (void *p)
{
    struct mb_cipc *self = (struct mb_cipc *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }

    mb_ep_stopped (self->ep);
}

static void mb_cipc_destroy (void *p)
{
    struct mb_cipc *self = (struct mb_cipc *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }

    mb_free (self);
}
