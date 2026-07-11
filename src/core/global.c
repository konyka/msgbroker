#include "../protocol.h"
#include "../transport.h"
#include "../utils/alloc.h"
#include "../utils/err.h"
#include "../utils/random.h"
#include "../memory/chunk.h"
#include "../memory/chunkref.h"
#include "../memory/msg.h"
#include "../pal/mutex.h"
#include "../pal/condvar.h"
#include "../pal/sleep.h"
#include "../distributed/cluster.h"
#include "../aio/coroutine.h"

#include "global.h"
#include "sock.h"
#include "ep.h"

#include <msgbroker/mb.h>
#include <mb_config.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MB_GLOBAL_STATE_IDLE           1
#define MB_GLOBAL_STATE_ACTIVE         2
#define MB_GLOBAL_STATE_STOPPING_TIMER 3

#define MB_CTX_FLAG_TERMED  1
#define MB_CTX_FLAG_TERMING 2
#define MB_CTX_FLAG_TERM    (MB_CTX_FLAG_TERMED | MB_CTX_FLAG_TERMING)

extern const struct mb_socktype mb_pair_socktype;
extern const struct mb_socktype mb_push_socktype;
extern const struct mb_socktype mb_pull_socktype;
extern const struct mb_socktype mb_req_socktype;
extern const struct mb_socktype mb_rep_socktype;
extern const struct mb_socktype mb_pub_socktype;
extern const struct mb_socktype mb_sub_socktype;
extern const struct mb_socktype mb_bus_socktype;
extern const struct mb_socktype mb_surveyor_socktype;
extern const struct mb_socktype mb_respondent_socktype;
extern const struct mb_socktype mb_xpair_socktype;
extern const struct mb_socktype mb_xpush_socktype;
extern const struct mb_socktype mb_xpull_socktype;
extern const struct mb_socktype mb_xreq_socktype;
extern const struct mb_socktype mb_xrep_socktype;
extern const struct mb_socktype mb_xpub_socktype;
extern const struct mb_socktype mb_xsub_socktype;
extern const struct mb_socktype mb_xbus_socktype;
extern const struct mb_socktype mb_xsurveyor_socktype;
extern const struct mb_socktype mb_xrespondent_socktype;
extern const struct mb_transport mb_inproc_transport;
extern const struct mb_transport mb_ipc_transport;
extern const struct mb_transport mb_tcp_transport;
extern const struct mb_transport mb_tls_transport;
extern const struct mb_transport mb_ws_transport;
extern const struct mb_transport mb_wss_transport;

static const struct mb_socktype *mb_socktypes[] = {
    &mb_pair_socktype,
    &mb_push_socktype,
    &mb_pull_socktype,
    &mb_req_socktype,
    &mb_rep_socktype,
    &mb_pub_socktype,
    &mb_sub_socktype,
    &mb_bus_socktype,
    &mb_surveyor_socktype,
    &mb_respondent_socktype,
    &mb_xpair_socktype,
    &mb_xpush_socktype,
    &mb_xpull_socktype,
    &mb_xreq_socktype,
    &mb_xrep_socktype,
    &mb_xpub_socktype,
    &mb_xsub_socktype,
    &mb_xbus_socktype,
    &mb_xsurveyor_socktype,
    &mb_xrespondent_socktype,
    NULL,
};

static const struct mb_transport *mb_transports[] = {
    &mb_inproc_transport,
    &mb_ipc_transport,
    &mb_tcp_transport,
    &mb_tls_transport,
    &mb_ws_transport,
    &mb_wss_transport,
    NULL,
};

struct mb_global {
    struct mb_sock **socks;
    uint16_t *unused;
    size_t nsocks;
    int flags;
    struct mb_pool pool;
    int state;
    int inited;
    struct mb_mutex lock;
    struct mb_condvar cond;
    struct mb_cluster cluster;
    int cluster_inited;
};

static struct mb_global g_self;

static void mb_global_init (void);
static void mb_global_term (void);
static int mb_global_create_ep (struct mb_sock *sock, const char *addr,
    int bind);
static int mb_global_create_socket (int domain, int protocol);

int mb_errno (void)
{
    return mb_err_errno ();
}

const char *mb_strerror (int errnum)
{
    return mb_err_strerror (errnum);
}

static void mb_global_init (void)
{
    int i;
    const struct mb_transport *tp;

    if (g_self.socks)
        return;

    mb_random_seed ();

    g_self.socks = (struct mb_sock **) mb_alloc (
        sizeof (struct mb_sock *) * MB_MAX_SOCKETS +
        sizeof (uint16_t) * MB_MAX_SOCKETS);
    if (!g_self.socks)
        return;

    for (i = 0; i != MB_MAX_SOCKETS; ++i)
        g_self.socks[i] = NULL;
    g_self.nsocks = 0;
    g_self.flags = 0;

    g_self.unused = (uint16_t *) (g_self.socks + MB_MAX_SOCKETS);
    for (i = 0; i != MB_MAX_SOCKETS; ++i)
        g_self.unused[i] = (uint16_t) (MB_MAX_SOCKETS - i - 1);

    for (i = 0; (tp = mb_transports[i]) != NULL; i++) {
        if (tp->init)
            tp->init ();
    }

    mb_pool_init (&g_self.pool);
}

static void mb_global_term (void)
{
    const struct mb_transport *tp;
    int i;

    if (!g_self.socks)
        return;
    if (g_self.nsocks > 0)
        return;

    mb_pool_term (&g_self.pool);

    for (i = 0; (tp = mb_transports[i]) != NULL; i++) {
        if (tp->term)
            tp->term ();
    }

    mb_free (g_self.socks);
    g_self.socks = NULL;
}

void mb_term (void)
{
    int i;

    if (!g_self.inited)
        return;

    mb_mutex_lock (&g_self.lock);
    g_self.flags |= MB_CTX_FLAG_TERMING;
    mb_mutex_unlock (&g_self.lock);

    for (i = 0; i < MB_MAX_SOCKETS; i++) {
        (void) mb_close (i);
    }

    mb_mutex_lock (&g_self.lock);
    g_self.flags |= MB_CTX_FLAG_TERMED;
    g_self.flags &= ~MB_CTX_FLAG_TERMING;
    mb_condvar_broadcast (&g_self.cond);
    mb_mutex_unlock (&g_self.lock);
}

int mb_version_major (void) { return MB_VERSION_MAJOR; }
int mb_version_minor (void) { return MB_VERSION_MINOR; }
int mb_version_patch (void) { return MB_VERSION_PATCH; }
const char *mb_version_string (void) { return MB_VERSION_STRING; }

static int mb_global_create_socket (int domain, int protocol)
{
    int s;
    int i;
    const struct mb_socktype *socktype;
    struct mb_sock *sock;

    if (domain != AF_MB && domain != AF_MB_RAW)
        return -EAFNOSUPPORT;

    if (g_self.nsocks >= MB_MAX_SOCKETS)
        return -EMFILE;

    s = g_self.unused[MB_MAX_SOCKETS - g_self.nsocks - 1];

    for (i = 0; (socktype = mb_socktypes[i]) != NULL; i++) {
        if (socktype->domain == domain && socktype->protocol == protocol) {
            sock = (struct mb_sock *) mb_alloc (sizeof (struct mb_sock));
            if (!sock)
                return -ENOMEM;
            int rc = mb_sock_init (sock, socktype, s);
            if (rc < 0) {
                mb_free (sock);
                return rc;
            }
            g_self.socks[s] = sock;
            ++g_self.nsocks;
            return s;
        }
    }

    return -EPROTONOSUPPORT;
}

int mb_socket (int domain, int protocol)
{
    int rc;

    if (!g_self.inited) {
        mb_mutex_init (&g_self.lock);
        mb_condvar_init (&g_self.cond);
        g_self.inited = 1;
    }

    mb_mutex_lock (&g_self.lock);

    if (g_self.flags & MB_CTX_FLAG_TERM) {
        mb_mutex_unlock (&g_self.lock);
        mb_err_set_errno (ETERM);
        return -1;
    }

    mb_global_init ();

    rc = mb_global_create_socket (domain, protocol);
    if (rc < 0) {
        mb_global_term ();
        mb_mutex_unlock (&g_self.lock);
        mb_err_set_errno (-rc);
        return -1;
    }

    mb_mutex_unlock (&g_self.lock);
    return rc;
}

int mb_close (int s)
{
    struct mb_sock *sock;

    mb_mutex_lock (&g_self.lock);

    if (s < 0 || s >= MB_MAX_SOCKETS || !g_self.socks[s]) {
        mb_mutex_unlock (&g_self.lock);
        mb_err_set_errno (EBADF);
        return -1;
    }

    sock = g_self.socks[s];
    /* Unpublish first so concurrent hold_socket fails with EBADF. */
    g_self.socks[s] = NULL;
    g_self.unused[MB_MAX_SOCKETS - g_self.nsocks] = (uint16_t) s;
    --g_self.nsocks;
    mb_mutex_unlock (&g_self.lock);

    /* Drop the two permanent refs, then wait for in-flight send/recv. */
    mb_sock_rele (sock);
    mb_sock_rele (sock);
    while (__atomic_load_n (&sock->holds, __ATOMIC_ACQUIRE) > 0)
        mb_msleep (1);

    mb_sock_stop (sock);
    mb_sock_term (sock);
    mb_free (sock);

    mb_mutex_lock (&g_self.lock);
    mb_global_term ();
    mb_mutex_unlock (&g_self.lock);
    return 0;
}

int mb_setsockopt (int s, int level, int option, const void *optval,
    size_t optvallen)
{
    int rc;
    struct mb_sock *sock;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    if (!optval && optvallen) {
        mb_global_rele_socket (sock);
        mb_err_set_errno (EFAULT);
        return -1;
    }

    rc = mb_sock_setopt (sock, level, option, optval, optvallen);
    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return 0;
}

int mb_getsockopt (int s, int level, int option, void *optval,
    size_t *optvallen)
{
    int rc;
    struct mb_sock *sock;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    if (!optval && optvallen) {
        mb_global_rele_socket (sock);
        mb_err_set_errno (EFAULT);
        return -1;
    }

    rc = mb_sock_getopt (sock, level, option, optval, optvallen);
    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return 0;
}

static const struct mb_transport *mb_global_find_transport (const char *addr)
{
    int i;
    const struct mb_transport *tp;
    const char *sep;

    sep = strstr (addr, "://");
    if (!sep)
        return NULL;

    for (i = 0; (tp = mb_transports[i]) != NULL; i++) {
        if ((size_t) (sep - addr) == strlen (tp->name) &&
            memcmp (addr, tp->name, sep - addr) == 0)
            return tp;
    }
    return NULL;
}

static int mb_global_create_ep (struct mb_sock *sock, const char *addr,
    int bind)
{
    int rc;
    const struct mb_transport *tp;
    struct mb_ep *ep;

    tp = mb_global_find_transport (addr);
    if (!tp)
        return -EPROTONOSUPPORT;

    ep = (struct mb_ep *) mb_alloc (sizeof (struct mb_ep));
    if (!ep)
        return -ENOMEM;

    rc = mb_ep_init (ep, 1, sock, sock->eid++, tp, bind, addr);
    if (rc < 0) {
        mb_free (ep);
        return rc;
    }

    mb_list_insert (&sock->eps, &ep->item, mb_list_end (&sock->eps));
    mb_ep_start (ep);

    return ep->eid;
}

int mb_bind (int s, const char *addr)
{
    int rc;
    struct mb_sock *sock;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    rc = mb_global_create_ep (sock, addr, 1);
    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return rc;
}

int mb_connect (int s, const char *addr)
{
    int rc;
    struct mb_sock *sock;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    rc = mb_global_create_ep (sock, addr, 0);
    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return rc;
}

int mb_shutdown (int s, int how)
{
    struct mb_sock *sock;
    int rc;

    (void) how;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    mb_sock_stop (sock);
    mb_global_rele_socket (sock);
    return 0;
}

int mb_send (int s, const void *buf, size_t len, int flags)
{
    int rc;
    struct mb_sock *sock;
    struct mb_msg msg;
    int timeout;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    timeout = sock->sndtimeo;

    mb_msg_init_data (&msg, buf, len);
    if (len > MB_CHUNKREF_MAX && !mb_chunkref_data (&msg.body)) {
        mb_msg_term (&msg);
        mb_global_rele_socket (sock);
        mb_err_set_errno (ENOMEM);
        return -1;
    }

    for (;;) {
        rc = mb_sock_send (sock, &msg);
        if (rc >= 0)
            break;
        if (rc != -EAGAIN)
            break;
        mb_msg_term (&msg);
        if (flags & MB_DONTWAIT || timeout == 0) {
            mb_global_rele_socket (sock);
            mb_err_set_errno (EAGAIN);
            return -1;
        }
        if (timeout > 0) {
            mb_msleep (1);
            timeout -= 1;
            if (timeout <= 0) {
                mb_global_rele_socket (sock);
                mb_err_set_errno (EAGAIN);
                return -1;
            }
        } else {
            mb_msleep (1);
        }
        mb_msg_init_data (&msg, buf, len);
        if (len > MB_CHUNKREF_MAX && !mb_chunkref_data (&msg.body)) {
            mb_global_rele_socket (sock);
            mb_err_set_errno (ENOMEM);
            return -1;
        }
    }

    if (rc < 0)
        mb_msg_term (&msg);

    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return (int) len;
}

int mb_recv (int s, void *buf, size_t len, int flags)
{
    int rc;
    struct mb_sock *sock;
    struct mb_msg msg;
    size_t msglen;
    int timeout;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    timeout = sock->rcvtimeo;

    for (;;) {
        mb_msg_init (&msg, 0);
        rc = mb_sock_recv (sock, &msg);

        if (rc >= 0)
            break;
        mb_msg_term (&msg);
        if (rc != -EAGAIN) {
            mb_global_rele_socket (sock);
            mb_err_set_errno (-rc);
            return -1;
        }
        if (flags & MB_DONTWAIT || timeout == 0) {
            mb_global_rele_socket (sock);
            mb_err_set_errno (EAGAIN);
            return -1;
        }
        if (timeout > 0) {
            mb_msleep (1);
            timeout -= 1;
            if (timeout <= 0) {
                mb_global_rele_socket (sock);
                mb_err_set_errno (EAGAIN);
                return -1;
            }
        } else {
            mb_msleep (1);
        }
    }

    mb_global_rele_socket (sock);

    msglen = mb_chunkref_size (&msg.body);
    if (msglen > 0) {
        size_t tocopy = msglen <= len ? msglen : len;
        memcpy (buf, mb_chunkref_data (&msg.body), tocopy);
    }
    mb_msg_term (&msg);
    return (int) msglen;
}

int mb_sendmsg (int s, const struct mb_msghdr *msghdr, int flags)
{
    int rc;
    struct mb_sock *sock;
    struct mb_msg msg;
    size_t total;
    int i;
    char *ptr;
    struct mb_cmsghdr *cmsg;

    (void) flags;

    if (!msghdr || msghdr->msg_iovlen < 0) {
        mb_err_set_errno (EINVAL);
        return -1;
    }

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    total = 0;
    for (i = 0; i < msghdr->msg_iovlen; i++)
        total += msghdr->msg_iov[i].iov_len;

    mb_msg_init (&msg, total);
    ptr = (char *) mb_chunkref_data (&msg.body);
    if (total > MB_CHUNKREF_MAX && !ptr) {
        mb_msg_term (&msg);
        mb_global_rele_socket (sock);
        mb_err_set_errno (ENOMEM);
        return -1;
    }
    for (i = 0; i < msghdr->msg_iovlen; i++) {
        if (msghdr->msg_iov[i].iov_len > 0) {
            memcpy (ptr, msghdr->msg_iov[i].iov_base,
                    msghdr->msg_iov[i].iov_len);
            ptr += msghdr->msg_iov[i].iov_len;
        }
    }

    if (msghdr->msg_control && msghdr->msg_controllen > 0) {
        for (cmsg = MB_CMSG_FIRSTHDR (msghdr); cmsg;
             cmsg = MB_CMSG_NXTHDR (msghdr, cmsg)) {
            if (cmsg->cmsg_level == PROTO_SP && cmsg->cmsg_type == SP_HDR) {
                size_t hdrlen = cmsg->cmsg_len - MB_CMSG_LEN (0);
                if (hdrlen > 0) {
                    mb_chunkref_term (&msg.sphdr);
                    mb_chunkref_init (&msg.sphdr, hdrlen);
                    memcpy (mb_chunkref_data (&msg.sphdr),
                        MB_CMSG_DATA (cmsg), hdrlen);
                }
            }
        }
    }

    rc = mb_sock_send (sock, &msg);
    if (rc < 0)
        mb_msg_term (&msg);

    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }
    return (int) total;
}

int mb_recvmsg (int s, struct mb_msghdr *msghdr, int flags)
{
    int rc;
    struct mb_sock *sock;
    struct mb_msg msg;
    size_t msglen;
    size_t written;
    int i;
    const char *ptr;
    size_t sphdrlen;
    struct mb_cmsghdr *cmsg;

    (void) flags;

    if (!msghdr || msghdr->msg_iovlen < 0) {
        mb_err_set_errno (EINVAL);
        return -1;
    }

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    mb_msg_init (&msg, 0);
    rc = mb_sock_recv (sock, &msg);

    mb_global_rele_socket (sock);

    if (rc < 0) {
        mb_msg_term (&msg);
        mb_err_set_errno (-rc);
        return -1;
    }

    msglen = mb_chunkref_size (&msg.body);
    ptr = (const char *) mb_chunkref_data (&msg.body);
    written = 0;
    for (i = 0; i < msghdr->msg_iovlen && written < msglen; i++) {
        size_t chunk = msghdr->msg_iov[i].iov_len;
        if (written + chunk > msglen)
            chunk = msglen - written;
        if (chunk > 0) {
            memcpy (msghdr->msg_iov[i].iov_base, ptr + written, chunk);
            written += chunk;
        }
    }

    if (msghdr->msg_control && msghdr->msg_controllen > 0) {
        sphdrlen = mb_chunkref_size (&msg.sphdr);
        if (sphdrlen > 0) {
            size_t needed = MB_CMSG_SPACE (sphdrlen);
            if (needed <= msghdr->msg_controllen) {
                cmsg = (struct mb_cmsghdr *) msghdr->msg_control;
                cmsg->cmsg_len = MB_CMSG_LEN (sphdrlen);
                cmsg->cmsg_level = PROTO_SP;
                cmsg->cmsg_type = SP_HDR;
                memcpy (MB_CMSG_DATA (cmsg),
                    mb_chunkref_data (&msg.sphdr), sphdrlen);
                msghdr->msg_controllen = needed;
            } else {
                msghdr->msg_controllen = 0;
            }
        } else {
            msghdr->msg_controllen = 0;
        }
    }

    mb_msg_term (&msg);
    return (int) msglen;
}

void *mb_allocmsg (size_t size)
{
    void *result;
    int rc;
    rc = mb_chunk_alloc (size, &result);
    if (rc == 0)
        return result;
    return NULL;
}

void *mb_reallocmsg (void *msg, size_t size)
{
    int rc;
    rc = mb_chunk_realloc (size, &msg);
    if (rc == 0)
        return msg;
    return NULL;
}

int mb_freemsg (void *msg)
{
    mb_chunk_free (msg);
    return 0;
}

struct mb_cmsghdr *mb_cmsg_nxthdr_ (const struct mb_msghdr *mhdr,
    const struct mb_cmsghdr *cmsg)
{
    const unsigned char *ptr;
    const unsigned char *end;
    struct mb_cmsghdr *next;

    if (!mhdr || !mhdr->msg_control || mhdr->msg_controllen < sizeof (struct mb_cmsghdr))
        return NULL;

    end = (const unsigned char *) mhdr->msg_control + mhdr->msg_controllen;

    if (!cmsg) {
        ptr = (const unsigned char *) mhdr->msg_control;
    } else {
        ptr = (const unsigned char *) cmsg + MB_CMSG_ALIGN_(cmsg->cmsg_len);
    }

    if (ptr + sizeof (struct mb_cmsghdr) > end)
        return NULL;

    next = (struct mb_cmsghdr *) ptr;
    if (ptr + MB_CMSG_ALIGN_(next->cmsg_len) > end)
        return NULL;

    return next;
}

uint64_t mb_get_statistic (int s, int stat)
{
    struct mb_sock *sock;
    int rc;

    rc = mb_global_hold_socket (&sock, s);
    if (rc < 0)
        return 0;

    uint64_t val = mb_sock_get_statistic (sock, stat);
    mb_global_rele_socket (sock);
    return val;
}

int mb_coro_send (int s, const void *buf, size_t len)
{
    int rc;
    struct mb_coro *coro;

    coro = mb_coro_current ();
    if (!coro)
        return mb_send (s, buf, len, 0);

    for (;;) {
        rc = mb_send (s, buf, len, MB_DONTWAIT);
        if (rc >= 0)
            return rc;
        if (mb_errno () != EAGAIN)
            return -1;
        mb_coro_yield (NULL);
    }
}

int mb_coro_recv (int s, void *buf, size_t len)
{
    int rc;
    struct mb_coro *coro;

    coro = mb_coro_current ();
    if (!coro)
        return mb_recv (s, buf, len, 0);

    for (;;) {
        rc = mb_recv (s, buf, len, MB_DONTWAIT);
        if (rc >= 0)
            return rc;
        if (mb_errno () != EAGAIN)
            return -1;
        mb_coro_yield (NULL);
    }
}

struct mb_pool *mb_global_pool (void)
{
    return &g_self.pool;
}

struct mb_ctx *mb_global_getctx (void)
{
    return NULL;
}

const struct mb_transport *mb_global_transport (int id)
{
    int i;
    const struct mb_transport *tp;
    for (i = 0; (tp = mb_transports[i]) != NULL; i++) {
        if (tp->id == id)
            return tp;
    }
    return NULL;
}

int mb_global_hold_socket (struct mb_sock **sockp, int s)
{
    mb_mutex_lock (&g_self.lock);
    if (s < 0 || s >= MB_MAX_SOCKETS || !g_self.socks[s]) {
        mb_mutex_unlock (&g_self.lock);
        return -EBADF;
    }
    *sockp = g_self.socks[s];
    mb_sock_hold (*sockp);
    mb_mutex_unlock (&g_self.lock);
    return 0;
}

void mb_global_rele_socket (struct mb_sock *sock)
{
    mb_sock_rele (sock);
}

int mb_cluster_join (int s, const char *cluster_addr)
{
    (void) s;
    if (!cluster_addr)
        return -EINVAL;

    mb_mutex_lock (&g_self.lock);
    if (!g_self.cluster_inited) {
        mb_cluster_init (&g_self.cluster, (uint32_t) getpid (),
            cluster_addr);
        g_self.cluster_inited = 1;
    }
    int rc = mb_cluster_start (&g_self.cluster);
    mb_mutex_unlock (&g_self.lock);
    return rc;
}

int mb_cluster_leave (int s)
{
    (void) s;
    mb_mutex_lock (&g_self.lock);
    if (g_self.cluster_inited) {
        mb_cluster_stop (&g_self.cluster);
    }
    mb_mutex_unlock (&g_self.lock);
    return 0;
}

int mb_cluster_route (int s, const void *key, size_t keylen)
{
    (void) s;
    if (!key || keylen == 0)
        return -EINVAL;

    mb_mutex_lock (&g_self.lock);
    uint32_t node_id = 0;
    if (g_self.cluster_inited) {
        node_id = mb_cluster_route_key (&g_self.cluster, key, keylen);
    }
    mb_mutex_unlock (&g_self.lock);
    return (int) node_id;
}

int mb_cluster_replicate (int s, int replica_count)
{
    (void) s; (void) replica_count;
    return 0;
}
