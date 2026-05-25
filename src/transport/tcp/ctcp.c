#include "ctcp.h"
#include "../ipc/sipc.h"
#include "../../core/ep.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

static void mb_ctcp_stop (void *p);
static void mb_ctcp_destroy (void *p);

static const struct mb_ep_ops mb_ctcp_ops = {
    mb_ctcp_stop,
    mb_ctcp_destroy,
};

static int mb_ctcp_parse_addr (const char *addr, char *host, size_t hostlen,
    uint16_t *port)
{
    const char *sep;
    const char *portstr;
    unsigned long p;

    sep = strstr (addr, "://");
    if (!sep)
        return -EINVAL;
    sep += 3;

    portstr = strrchr (sep, ':');
    if (!portstr)
        return -EINVAL;

    if ((size_t) (portstr - sep) >= hostlen)
        return -EINVAL;

    memcpy (host, sep, (size_t) (portstr - sep));
    host[portstr - sep] = '\0';
    portstr++;

    p = strtoul (portstr, NULL, 10);
    if (p == 0 || p > 65535)
        return -EINVAL;
    *port = (uint16_t) p;
    return 0;
}

int mb_ctcp_create (struct mb_ep *ep)
{
    struct mb_ctcp *self;
    struct sockaddr_in sa;
    int fd;
    int rc;
    char host[256];
    uint16_t port;
    int flag = 1;

    rc = mb_ctcp_parse_addr (mb_ep_getaddr (ep), host, sizeof (host), &port);
    if (rc < 0)
        return rc;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof (flag));

    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons (port);

    if (inet_pton (AF_INET, host, &sa.sin_addr) <= 0) {
        close (fd);
        return -EINVAL;
    }

    rc = connect (fd, (struct sockaddr *) &sa, sizeof (sa));
    if (rc < 0) {
        close (fd);
        return -errno;
    }

    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

    self = (struct mb_ctcp *) mb_alloc (sizeof (struct mb_ctcp));
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

    mb_ep_tran_setup (ep, &mb_ctcp_ops, self);

    return 0;
}

static void mb_ctcp_stop (void *p)
{
    struct mb_ctcp *self = (struct mb_ctcp *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }

    mb_ep_stopped (self->ep);
}

static void mb_ctcp_destroy (void *p)
{
    struct mb_ctcp *self = (struct mb_ctcp *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }

    mb_free (self);
}
