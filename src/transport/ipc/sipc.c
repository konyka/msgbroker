#include "sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/err.h"
#include "../../utils/wire.h"
#include "../../memory/msg.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MB_SIPC_INSTATE_HDR  1
#define MB_SIPC_INSTATE_BODY 2
#define MB_SIPC_INSTATE_HASMSG 3

static int mb_sipc_send (struct mb_pipebase *base, struct mb_msg *msg);
static int mb_sipc_recv (struct mb_pipebase *base, struct mb_msg *msg);

static const struct mb_pipebase_vfptr mb_sipc_vfptr = {
    mb_sipc_send,
    mb_sipc_recv,
};

static int mb_sipc_send_fd (int fd, const void *buf, size_t len)
{
    const uint8_t *ptr = (const uint8_t *) buf;
    size_t remaining = len;
    size_t sent = 0;

    while (remaining > 0) {
        ssize_t ns = send (fd, ptr, remaining, MSG_NOSIGNAL);
        if (ns < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return sent > 0 ? (int) sent : -EAGAIN;
            return -errno;
        }
        ptr += ns;
        remaining -= (size_t) ns;
        sent += (size_t) ns;
    }
    return (int) sent;
}

static int mb_sipc_recv_fd (int fd, void *buf, size_t len)
{
    uint8_t *ptr = (uint8_t *) buf;
    size_t remaining = len;
    size_t got = 0;

    while (remaining > 0) {
        ssize_t nr = recv (fd, ptr, remaining, 0);
        if (nr <= 0) {
            if (nr == 0)
                return got > 0 ? (int) got : -ECONNRESET;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return got > 0 ? (int) got : -EAGAIN;
            return -errno;
        }
        ptr += nr;
        remaining -= (size_t) nr;
        got += (size_t) nr;
    }
    return (int) got;
}

int mb_sipc_create (struct mb_sipc *self, struct mb_ep *ep, int fd)
{
    self->fd = fd;
    mb_pipebase_init (&self->pipebase, &mb_sipc_vfptr, ep);
    mb_list_item_init (&self->item);
    self->inpos = 0;
    self->inlen = 0;
    self->instate = MB_SIPC_INSTATE_HDR;
    self->outbuf = NULL;
    self->outpos = 0;
    self->outlen = 0;
    self->disconnected = 0;
    self->on_error = NULL;
    self->on_error_arg = NULL;
    mb_msg_init (&self->inmsg, 0);
    return 0;
}

void mb_sipc_term (struct mb_sipc *self)
{
    mb_msg_term (&self->inmsg);
    mb_list_item_term (&self->item);
    mb_pipebase_term (&self->pipebase);
    if (self->outbuf) {
        mb_free (self->outbuf);
        self->outbuf = NULL;
    }
    if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }
}

int mb_sipc_start (struct mb_sipc *self)
{
    return mb_pipebase_start (&self->pipebase);
}

void mb_sipc_stop (struct mb_sipc *self)
{
    if (self->pipebase.state == 2)
        mb_pipebase_stop (&self->pipebase);
    if (self->outbuf) {
        mb_free (self->outbuf);
        self->outbuf = NULL;
        self->outpos = 0;
        self->outlen = 0;
    }
    if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }
}

void mb_sipc_set_on_error (struct mb_sipc *self, void (*cb) (void *), void *arg)
{
    self->on_error = cb;
    self->on_error_arg = arg;
}

static void mb_sipc_report_error (struct mb_sipc *self)
{
    void (*cb) (void *);
    void *arg;

    if (self->disconnected)
        return;
    self->disconnected = 1;
    cb = self->on_error;
    arg = self->on_error_arg;
    self->on_error = NULL;
    if (cb)
        cb (arg);
}

static int mb_sipc_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sipc *self = mb_cont (base, struct mb_sipc, pipebase);
    int rc;

    if (self->fd < 0) {
        mb_sipc_report_error (self);
        return -ECONNRESET;
    }

    /* Flush any previously accepted message before taking a new one. */
    if (self->outbuf) {
        while (self->outpos < self->outlen) {
            rc = mb_sipc_send_fd (self->fd, self->outbuf + self->outpos,
                (size_t) (self->outlen - self->outpos));
            if (rc < 0) {
                if (rc != -EAGAIN)
                    mb_sipc_report_error (self);
                return rc;
            }
            self->outpos += rc;
        }
        mb_free (self->outbuf);
        self->outbuf = NULL;
        self->outpos = 0;
        self->outlen = 0;
    }

    {
        size_t body_size = mb_chunkref_size (&msg->body);

        self->outlen = (int) (MB_SIPC_HDR_SIZE + body_size);
        self->outbuf = (uint8_t *) mb_alloc ((size_t) self->outlen);
        if (!self->outbuf)
            return -ENOMEM;
        mb_wire_put_uint32 (self->outbuf, (uint32_t) body_size);
        if (body_size > 0)
            memcpy (self->outbuf + MB_SIPC_HDR_SIZE,
                mb_chunkref_data (&msg->body), body_size);
        self->outpos = 0;

        /* Accepted: EAGAIN means pending wire flush, not "msg not taken". */
        mb_msg_term (msg);
        mb_msg_init (msg, 0);
    }

    while (self->outpos < self->outlen) {
        rc = mb_sipc_send_fd (self->fd, self->outbuf + self->outpos,
            (size_t) (self->outlen - self->outpos));
        if (rc < 0) {
            if (rc != -EAGAIN)
                mb_sipc_report_error (self);
            else
                return 0;
            return rc;
        }
        self->outpos += rc;
    }

    mb_free (self->outbuf);
    self->outbuf = NULL;
    self->outpos = 0;
    self->outlen = 0;
    return 0;
}

static int mb_sipc_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sipc *self = mb_cont (base, struct mb_sipc, pipebase);
    int rc;
    struct pollfd pfd;
    uint32_t body_size;

    if (self->fd < 0) {
        mb_sipc_report_error (self);
        return -ECONNRESET;
    }

    if (self->instate == MB_SIPC_INSTATE_HASMSG) {
        mb_msg_mv (msg, &self->inmsg);
        mb_msg_init (&self->inmsg, 0);
        self->instate = MB_SIPC_INSTATE_HDR;
        self->inpos = 0;
        return 0;
    }

    pfd.fd = self->fd;
    pfd.events = POLLIN;
    rc = poll (&pfd, 1, 0);
    if (rc <= 0)
        return -EAGAIN;

    if (self->instate == MB_SIPC_INSTATE_HDR) {
        rc = mb_sipc_recv_fd (self->fd, self->inhdr + self->inpos,
            MB_SIPC_HDR_SIZE - self->inpos);
        if (rc < 0) {
            if (rc == -EAGAIN)
                return -EAGAIN;
            mb_sipc_report_error (self);
            return rc;
        }
        self->inpos += rc;
        if (self->inpos < MB_SIPC_HDR_SIZE)
            return -EAGAIN;

        body_size = mb_wire_get_uint32 (self->inhdr);
        if (body_size > 1024 * 1024) {
            mb_sipc_report_error (self);
            return -EMSGSIZE;
        }

        mb_msg_term (&self->inmsg);
        mb_msg_init (&self->inmsg, (size_t) body_size);
        self->inlen = (int) body_size;
        self->inpos = 0;
        self->instate = MB_SIPC_INSTATE_BODY;
    }

    if (self->instate == MB_SIPC_INSTATE_BODY) {
        if (self->inlen > 0) {
            rc = mb_sipc_recv_fd (self->fd,
                mb_chunkref_data (&self->inmsg.body) + self->inpos,
                (size_t) (self->inlen - self->inpos));
            if (rc < 0) {
                if (rc == -EAGAIN)
                    return -EAGAIN;
                mb_sipc_report_error (self);
                return rc;
            }
            self->inpos += rc;
            if (self->inpos < self->inlen)
                return -EAGAIN;
        }
        self->instate = MB_SIPC_INSTATE_HASMSG;
    }

    mb_msg_mv (msg, &self->inmsg);
    mb_msg_init (&self->inmsg, 0);
    self->instate = MB_SIPC_INSTATE_HDR;
    self->inpos = 0;
    return 0;
}
