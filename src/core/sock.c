#include "../protocol.h"
#include "../transport.h"
#include "../memory/msg.h"
#include "../aio/pool.h"
#include "../utils/alloc.h"
#include "../utils/cont.h"

#include "sock.h"
#include "global.h"
#include "ep.h"
#include "pipe.h"

#include <msgbroker/mb_tls.h>

#include <stdio.h>
#include <string.h>

static void mb_sock_handler (struct mb_fsm *self, int src, int type,
    void *srcptr);
static void mb_sock_shutdown (struct mb_fsm *self, int src, int type,
    void *srcptr);
static void mb_sock_onleave (struct mb_ctx *ctx);

int mb_sock_init (struct mb_sock *self, const struct mb_socktype *socktype,
    int fd)
{
    int rc;
    int i;

    mb_fsm_init_root (&self->fsm, mb_sock_handler, mb_sock_shutdown,
        mb_global_getctx ());
    self->state = MB_SOCK_STATE_INIT;

    mb_efd_init (&self->sndfd);
    mb_efd_init (&self->rcvfd);
    mb_sem_init (&self->termsem);

    mb_ctx_init (&self->ctx, mb_global_pool (), mb_sock_onleave);

    mb_list_init (&self->eps);
    mb_list_init (&self->sdeps);

    self->socktype = socktype;
    self->flags = 0;
    self->eid = 1;
    self->holds = 2;

    self->sndbuf = 128 * 1024;
    self->rcvbuf = 128 * 1024;
    self->rcvmaxsize = 1024 * 1024;
    self->sndtimeo = -1;
    self->rcvtimeo = -1;
    self->reconnect_ivl = 100;
    self->reconnect_ivl_max = 0;
    self->maxttl = 8;
    self->linger = 1000;

    self->ep_template.sndprio = 8;
    self->ep_template.rcvprio = 8;
    self->ep_template.ipv4only = 0;

    for (i = 0; i < MB_MAX_TRANSPORT; ++i)
        self->optsets[i] = NULL;

    memset (&self->statistics, 0, sizeof (self->statistics));
    snprintf (self->socket_name, sizeof (self->socket_name), "%d", fd);

    memset (self->tls_cert_path, 0, sizeof (self->tls_cert_path));
    memset (self->tls_key_path, 0, sizeof (self->tls_key_path));
    memset (self->tls_ca_path, 0, sizeof (self->tls_ca_path));
    self->tls_verify = 0;

    rc = socktype->create ((void *) (intptr_t) fd, &self->sockbase);
    if (rc < 0) {
        mb_ctx_term (&self->ctx);
        mb_sem_term (&self->termsem);
        mb_efd_term (&self->rcvfd);
        mb_efd_term (&self->sndfd);
        mb_fsm_term (&self->fsm);
        return rc;
    }

    self->state = MB_SOCK_STATE_ACTIVE;
    mb_fsm_start (&self->fsm);

    return 0;
}

void mb_sock_stop (struct mb_sock *self)
{
    mb_ctx_enter (&self->ctx);
    mb_fsm_stop (&self->fsm);
    mb_ctx_leave (&self->ctx);
}

int mb_sock_term (struct mb_sock *self)
{
    mb_efd_term (&self->rcvfd);
    mb_efd_term (&self->sndfd);
    mb_sem_term (&self->termsem);
    mb_fsm_term (&self->fsm);
    mb_ctx_term (&self->ctx);
    return 0;
}

void mb_sock_stopped (struct mb_sock *self)
{
    mb_sem_post (&self->termsem);
}

struct mb_ctx *mb_sock_getctx (struct mb_sock *self)
{
    return &self->ctx;
}

int mb_sock_ispeer (struct mb_sock *self, int socktype)
{
    return self->socktype->ispeer (socktype);
}

static void mb_sock_onleave (struct mb_ctx *ctx)
{
    (void) ctx;
}

int mb_sock_setopt (struct mb_sock *self, int level, int option,
    const void *optval, size_t optvallen)
{
    int rc;

    if (level == MB_SOL_SOCKET) {
        if (optvallen != sizeof (int))
            return -EINVAL;

        switch (option) {
        case MB_SNDBUF:           self->sndbuf = *(const int *)optval; return 0;
        case MB_RCVBUF:           self->rcvbuf = *(const int *)optval; return 0;
        case MB_RCVMAXSIZE:       self->rcvmaxsize = *(const int *)optval; return 0;
        case MB_SNDTIMEO:         self->sndtimeo = *(const int *)optval; return 0;
        case MB_RCVTIMEO:         self->rcvtimeo = *(const int *)optval; return 0;
        case MB_RECONNECT_IVL:    self->reconnect_ivl = *(const int *)optval; return 0;
        case MB_RECONNECT_IVL_MAX:self->reconnect_ivl_max = *(const int *)optval; return 0;
        case MB_SNDPRIO:          self->ep_template.sndprio = *(const int *)optval; return 0;
        case MB_RCVPRIO:          self->ep_template.rcvprio = *(const int *)optval; return 0;
        case MB_MAXTTL:           self->maxttl = *(const int *)optval; return 0;
        case MB_LINGER:           self->linger = *(const int *)optval; return 0;
        case MB_SOCKET_NAME:
            if (optvallen > sizeof (self->socket_name)) return -EINVAL;
            memcpy (self->socket_name, optval, optvallen);
            if (optvallen < sizeof (self->socket_name))
                self->socket_name[optvallen] = '\0';
            return 0;
        }
    }

    if (level == MB_TLS) {
        switch (option) {
        case MB_TLS_CONFIG_CERT:
            if (optvallen >= sizeof (self->tls_cert_path)) return -EINVAL;
            memcpy (self->tls_cert_path, optval, optvallen);
            self->tls_cert_path[optvallen] = '\0';
            return 0;
        case MB_TLS_CONFIG_KEY:
            if (optvallen >= sizeof (self->tls_key_path)) return -EINVAL;
            memcpy (self->tls_key_path, optval, optvallen);
            self->tls_key_path[optvallen] = '\0';
            return 0;
        case MB_TLS_CONFIG_CA:
            if (optvallen >= sizeof (self->tls_ca_path)) return -EINVAL;
            memcpy (self->tls_ca_path, optval, optvallen);
            self->tls_ca_path[optvallen] = '\0';
            return 0;
        case MB_TLS_CONFIG_VERIFY:
            if (optvallen != sizeof (int)) return -EINVAL;
            self->tls_verify = *(const int *)optval;
            return 0;
        }
    }

    if (self->sockbase && self->sockbase->vfptr->setopt) {
        rc = self->sockbase->vfptr->setopt (self->sockbase, level, option,
            optval, optvallen);
        if (rc != -ENOPROTOOPT)
            return rc;
    }

    return -ENOPROTOOPT;
}

int mb_sock_getopt (struct mb_sock *self, int level, int option,
    void *optval, size_t *optvallen)
{
    mb_ctx_enter (&self->ctx);
    mb_sock_getopt_inner (self, level, option, optval, optvallen);
    mb_ctx_leave (&self->ctx);
    return 0;
}

void mb_sock_getopt_inner (struct mb_sock *self, int level, int option,
    void *optval, size_t *optvallen)
{
    int val = 0;

    if (level == MB_SOL_SOCKET) {
        switch (option) {
        case MB_SNDBUF:            val = self->sndbuf; break;
        case MB_RCVBUF:            val = self->rcvbuf; break;
        case MB_RCVMAXSIZE:        val = self->rcvmaxsize; break;
        case MB_SNDTIMEO:          val = self->sndtimeo; break;
        case MB_RCVTIMEO:          val = self->rcvtimeo; break;
        case MB_RECONNECT_IVL:     val = self->reconnect_ivl; break;
        case MB_RECONNECT_IVL_MAX: val = self->reconnect_ivl_max; break;
        case MB_SNDPRIO:           val = self->ep_template.sndprio; break;
        case MB_RCVPRIO:           val = self->ep_template.rcvprio; break;
        case MB_MAXTTL:            val = self->maxttl; break;
        case MB_LINGER:            val = self->linger; break;
        case MB_DOMAIN:            val = self->socktype->domain; break;
        case MB_PROTOCOL:          val = self->socktype->protocol; break;
        case MB_SNDFD:             val = mb_efd_getfd (&self->sndfd); break;
        case MB_RCVFD:             val = mb_efd_getfd (&self->rcvfd); break;
        case MB_SOCKET_NAME:
            if (*optvallen >= sizeof (self->socket_name)) {
                memcpy (optval, self->socket_name, sizeof (self->socket_name));
                *optvallen = sizeof (self->socket_name);
            } else {
                memcpy (optval, self->socket_name, *optvallen);
            }
            return;
        default: return;
        }
        if (*optvallen >= sizeof (int)) {
            memcpy (optval, &val, sizeof (int));
            *optvallen = sizeof (int);
        }
    }
}

int mb_sock_send (struct mb_sock *self, struct mb_msg *msg)
{
    int rc;

    if (self->socktype->flags & MB_SOCKTYPE_FLAG_NOSEND)
        return -EOPNOTSUPP;

    mb_ctx_enter (&self->ctx);
    rc = self->sockbase->vfptr->send (self->sockbase, msg);
    if (rc >= 0) {
        self->statistics.messages_sent++;
        self->statistics.bytes_sent += mb_chunkref_size (&msg->body);
    }
    mb_ctx_leave (&self->ctx);
    return rc;
}

int mb_sock_recv (struct mb_sock *self, struct mb_msg *msg)
{
    int rc;

    if (self->socktype->flags & MB_SOCKTYPE_FLAG_NORECV)
        return -EOPNOTSUPP;

    mb_ctx_enter (&self->ctx);
    rc = self->sockbase->vfptr->recv (self->sockbase, msg);
    if (rc >= 0) {
        self->statistics.messages_received++;
        self->statistics.bytes_received += mb_chunkref_size (&msg->body);
    }
    mb_ctx_leave (&self->ctx);
    return rc;
}

int mb_sock_pipe_add (struct mb_sock *self, struct mb_pipe *pipe)
{
    return self->sockbase->vfptr->add (self->sockbase, pipe);
}

void mb_sock_pipe_rm (struct mb_sock *self, struct mb_pipe *pipe)
{
    self->sockbase->vfptr->rm (self->sockbase, pipe);
}

void mb_sock_stat_increment (struct mb_sock *self, int name, int increment)
{
    switch (name) {
    case MB_STAT_ESTABLISHED_CONNECTIONS:
        self->statistics.established_connections += (uint64_t)increment; break;
    case MB_STAT_ACCEPTED_CONNECTIONS:
        self->statistics.accepted_connections += (uint64_t)increment; break;
    case MB_STAT_DROPPED_CONNECTIONS:
        self->statistics.dropped_connections += (uint64_t)increment; break;
    case MB_STAT_BROKEN_CONNECTIONS:
        self->statistics.broken_connections += (uint64_t)increment; break;
    case MB_STAT_CONNECT_ERRORS:
        self->statistics.connect_errors += (uint64_t)increment; break;
    case MB_STAT_BIND_ERRORS:
        self->statistics.bind_errors += (uint64_t)increment; break;
    case MB_STAT_ACCEPT_ERRORS:
        self->statistics.accept_errors += (uint64_t)increment; break;
    case MB_STAT_CURRENT_CONNECTIONS:
        self->statistics.current_connections += increment; break;
    case MB_STAT_INPROGRESS_CONNECTIONS:
        self->statistics.inprogress_connections += increment; break;
    case MB_STAT_CURRENT_EP_ERRORS:
        self->statistics.current_ep_errors += increment; break;
    }
}

uint64_t mb_sock_get_statistic (struct mb_sock *self, int stat)
{
    switch (stat) {
    case MB_STAT_ESTABLISHED_CONNECTIONS: return self->statistics.established_connections;
    case MB_STAT_ACCEPTED_CONNECTIONS:    return self->statistics.accepted_connections;
    case MB_STAT_DROPPED_CONNECTIONS:     return self->statistics.dropped_connections;
    case MB_STAT_BROKEN_CONNECTIONS:      return self->statistics.broken_connections;
    case MB_STAT_CONNECT_ERRORS:          return self->statistics.connect_errors;
    case MB_STAT_BIND_ERRORS:             return self->statistics.bind_errors;
    case MB_STAT_ACCEPT_ERRORS:           return self->statistics.accept_errors;
    case MB_STAT_CURRENT_CONNECTIONS:     return (uint64_t)self->statistics.current_connections;
    case MB_STAT_INPROGRESS_CONNECTIONS:  return (uint64_t)self->statistics.inprogress_connections;
    case MB_STAT_MESSAGES_SENT:           return self->statistics.messages_sent;
    case MB_STAT_MESSAGES_RECEIVED:       return self->statistics.messages_received;
    case MB_STAT_BYTES_SENT:              return self->statistics.bytes_sent;
    case MB_STAT_BYTES_RECEIVED:          return self->statistics.bytes_received;
    case MB_STAT_CURRENT_SND_PRIORITY:    return (uint64_t)self->statistics.current_snd_priority;
    case MB_STAT_CURRENT_EP_ERRORS:       return (uint64_t)self->statistics.current_ep_errors;
    }
    return 0;
}

int mb_sock_hold (struct mb_sock *self) { self->holds++; return 0; }
void mb_sock_rele (struct mb_sock *self) { self->holds--; }

static void mb_sock_handler (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_sock *self = (struct mb_sock *)fsm;
    (void)src; (void)type; (void)srcptr;

    switch (self->state) {
    case MB_SOCK_STATE_ACTIVE:
        break;
    case MB_SOCK_STATE_STOPPING_EPS:
        if (!mb_list_empty (&self->eps) || !mb_list_empty (&self->sdeps))
            break;
        self->state = MB_SOCK_STATE_STOPPING;
        if (self->sockbase->vfptr->stop)
            self->sockbase->vfptr->stop (self->sockbase);
        break;
    case MB_SOCK_STATE_STOPPING:
        self->sockbase->vfptr->destroy (self->sockbase);
        self->sockbase = NULL;
        self->state = MB_SOCK_STATE_INIT;
        mb_sock_stopped (self);
        break;
    }
}

static void mb_sock_shutdown (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_sock *self = (struct mb_sock *)fsm;
    (void)src; (void)type; (void)srcptr;

    if (self->state == MB_SOCK_STATE_ACTIVE) {
        self->state = MB_SOCK_STATE_STOPPING_EPS;

        /* Synchronously stop/destroy all endpoints. The async
         * MB_EP_STOPPED path is never drained by the current ctx
         * event loop, so close() would otherwise leak listen fds
         * and leave ports bound forever. */
        while (!mb_list_empty (&self->eps)) {
            struct mb_list_item *it = mb_list_begin (&self->eps);
            struct mb_ep *ep = mb_cont (it, struct mb_ep, item);
            mb_list_erase (&self->eps, it);
            mb_ep_stop (ep);
            mb_ep_term (ep);
            mb_free (ep);
        }
        while (!mb_list_empty (&self->sdeps)) {
            struct mb_list_item *it = mb_list_begin (&self->sdeps);
            struct mb_ep *ep = mb_cont (it, struct mb_ep, item);
            mb_list_erase (&self->sdeps, it);
            mb_ep_stop (ep);
            mb_ep_term (ep);
            mb_free (ep);
        }

        self->state = MB_SOCK_STATE_STOPPING;
        if (self->sockbase && self->sockbase->vfptr->stop)
            self->sockbase->vfptr->stop (self->sockbase);
        if (self->sockbase)
            self->sockbase->vfptr->destroy (self->sockbase);
        self->sockbase = NULL;
        self->state = MB_SOCK_STATE_INIT;
        mb_sock_stopped (self);
    }
}
