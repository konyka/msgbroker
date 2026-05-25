#include "btcp.h"
#include "../ipc/sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"

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
#include <poll.h>

#define MB_BTCP_BACKLOG 10

static void mb_btcp_stop (void *p);
static void mb_btcp_destroy (void *p);

static const struct mb_ep_ops mb_btcp_ops = {
    mb_btcp_stop,
    mb_btcp_destroy,
};

static int mb_tcp_parse_addr (const char *addr, char *host, size_t hostlen,
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

static void mb_btcp_accept_loop (void *arg)
{
    struct mb_btcp *self = (struct mb_btcp *) arg;

    while (self->running) {
        struct pollfd pfd;
        int rc;

        pfd.fd = self->listen_fd;
        pfd.events = POLLIN;
        rc = poll (&pfd, 1, 100);

        if (rc <= 0)
            continue;

        if (pfd.revents & POLLIN) {
            struct sockaddr_in client;
            socklen_t client_len = sizeof (client);
            int client_fd;
            struct mb_sipc *sipc;
            int flag = 1;

            client_fd = accept (self->listen_fd,
                (struct sockaddr *) &client, &client_len);
            if (client_fd < 0)
                continue;

            setsockopt (client_fd, IPPROTO_TCP, TCP_NODELAY,
                &flag, sizeof (flag));
            fcntl (client_fd, F_SETFL,
                fcntl (client_fd, F_GETFL, 0) | O_NONBLOCK);

            sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
            if (!sipc) {
                close (client_fd);
                continue;
            }

            mb_sipc_create (sipc, self->ep, client_fd);

            mb_mutex_lock (&self->lock);
            mb_sipc_start (sipc);
            mb_list_insert (&self->sipcs, &sipc->item,
                mb_list_end (&self->sipcs));
            mb_mutex_unlock (&self->lock);
        }
    }
}

int mb_btcp_create (struct mb_ep *ep)
{
    struct mb_btcp *self;
    struct sockaddr_in sa;
    int fd;
    int rc;
    char host[256];
    uint16_t port;
    int flag = 1;

    rc = mb_tcp_parse_addr (mb_ep_getaddr (ep), host, sizeof (host), &port);
    if (rc < 0)
        return rc;

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag));

    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons (port);

    if (strcmp (host, "*") == 0 || strcmp (host, "0.0.0.0") == 0) {
        sa.sin_addr.s_addr = htonl (INADDR_ANY);
    } else {
        if (inet_pton (AF_INET, host, &sa.sin_addr) <= 0) {
            close (fd);
            return -EINVAL;
        }
    }

    rc = bind (fd, (struct sockaddr *) &sa, sizeof (sa));
    if (rc < 0) {
        close (fd);
        return -errno;
    }

    rc = listen (fd, MB_BTCP_BACKLOG);
    if (rc < 0) {
        close (fd);
        return -errno;
    }

    self = (struct mb_btcp *) mb_alloc (sizeof (struct mb_btcp));
    if (!self) {
        close (fd);
        return -ENOMEM;
    }

    self->ep = ep;
    self->listen_fd = fd;
    mb_list_init (&self->sipcs);
    mb_mutex_init (&self->lock);
    self->running = 1;

    mb_ep_tran_setup (ep, &mb_btcp_ops, self);

    mb_thread_init (&self->accept_thread);
    mb_thread_start (&self->accept_thread, mb_btcp_accept_loop, self);

    return 0;
}

static void mb_btcp_cleanup (struct mb_btcp *self)
{
    while (!mb_list_empty (&self->sipcs)) {
        struct mb_list_item *it = mb_list_begin (&self->sipcs);
        struct mb_sipc *sipc = (struct mb_sipc *) it;
        mb_list_erase (&self->sipcs, it);
        mb_sipc_stop (sipc);
        mb_sipc_term (sipc);
        mb_free (sipc);
    }

    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        self->listen_fd = -1;
    }
}

static void mb_btcp_stop (void *p)
{
    struct mb_btcp *self = (struct mb_btcp *) p;

    self->running = 0;
    mb_thread_term (&self->accept_thread);

    mb_mutex_lock (&self->lock);
    mb_btcp_cleanup (self);
    mb_mutex_unlock (&self->lock);

    mb_ep_stopped (self->ep);
}

static void mb_btcp_destroy (void *p)
{
    struct mb_btcp *self = (struct mb_btcp *) p;

    self->running = 0;
    mb_btcp_cleanup (self);

    mb_mutex_term (&self->lock);
    mb_list_term (&self->sipcs);
    mb_free (self);
}
