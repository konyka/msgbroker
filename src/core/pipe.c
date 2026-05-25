#include "../transport.h"
#include "sock.h"
#include "ep.h"
#include "../utils/alloc.h"
#include "../utils/err.h"
#include "../utils/fast.h"

#include <assert.h>
#include <string.h>

#define MB_PIPEBASE_STATE_IDLE    1
#define MB_PIPEBASE_STATE_ACTIVE  2
#define MB_PIPEBASE_STATE_FAILED  3

#define MB_PIPEBASE_INSTATE_DEACTIVATED 0
#define MB_PIPEBASE_INSTATE_IDLE        1
#define MB_PIPEBASE_INSTATE_RECEIVING   2
#define MB_PIPEBASE_INSTATE_RECEIVED    3
#define MB_PIPEBASE_INSTATE_ASYNC       4

#define MB_PIPEBASE_OUTSTATE_DEACTIVATED 0
#define MB_PIPEBASE_OUTSTATE_IDLE        1
#define MB_PIPEBASE_OUTSTATE_SENDING     2
#define MB_PIPEBASE_OUTSTATE_SENT        3
#define MB_PIPEBASE_OUTSTATE_ASYNC       4

static void mb_pipebase_fsm_handler (struct mb_fsm *self, int src, int type,
    void *srcptr)
{
    (void) self; (void) src; (void) type; (void) srcptr;
}

void mb_pipebase_init (struct mb_pipebase *self,
    const struct mb_pipebase_vfptr *vfptr, struct mb_ep *ep)
{
    mb_fsm_init (&self->fsm, mb_pipebase_fsm_handler, NULL, 0, self,
        &mb_ep_sock (ep)->fsm);
    self->vfptr = vfptr;
    self->state = MB_PIPEBASE_STATE_IDLE;
    self->instate = MB_PIPEBASE_INSTATE_DEACTIVATED;
    self->outstate = MB_PIPEBASE_OUTSTATE_DEACTIVATED;
    self->sock = mb_ep_sock (ep);
    memcpy (&self->options, &ep->options, sizeof (struct mb_ep_options));
    mb_fsm_event_init (&self->in);
    mb_fsm_event_init (&self->out);
}

void mb_pipebase_term (struct mb_pipebase *self)
{
    mb_fsm_event_term (&self->out);
    mb_fsm_event_term (&self->in);
    mb_fsm_term (&self->fsm);
}

int mb_pipebase_start (struct mb_pipebase *self)
{
    int rc;

    self->state = MB_PIPEBASE_STATE_ACTIVE;
    self->instate = MB_PIPEBASE_INSTATE_IDLE;
    self->outstate = MB_PIPEBASE_OUTSTATE_IDLE;
    mb_fsm_start (&self->fsm);

    rc = mb_sock_pipe_add (self->sock, (struct mb_pipe *) self);
    if (rc < 0) {
        self->state = MB_PIPEBASE_STATE_FAILED;
        self->instate = MB_PIPEBASE_INSTATE_DEACTIVATED;
        self->outstate = MB_PIPEBASE_OUTSTATE_DEACTIVATED;
        return rc;
    }

    mb_pipebase_received (self);
    mb_pipebase_sent (self);
    return 0;
}

void mb_pipebase_stop (struct mb_pipebase *self)
{
    assert (self->state == MB_PIPEBASE_STATE_ACTIVE);
    mb_sock_pipe_rm (self->sock, (struct mb_pipe *) self);
    self->state = MB_PIPEBASE_STATE_IDLE;
    self->instate = MB_PIPEBASE_INSTATE_DEACTIVATED;
    self->outstate = MB_PIPEBASE_OUTSTATE_DEACTIVATED;
}

void mb_pipebase_received (struct mb_pipebase *self)
{
    if (self->state != MB_PIPEBASE_STATE_ACTIVE)
        return;
    if (self->instate == MB_PIPEBASE_INSTATE_ASYNC)
        return;
    self->instate = MB_PIPEBASE_INSTATE_IDLE;
    mb_fsm_event_start (&self->in, MB_PIPE_IN);
}

void mb_pipebase_sent (struct mb_pipebase *self)
{
    if (self->state != MB_PIPEBASE_STATE_ACTIVE)
        return;
    if (self->outstate == MB_PIPEBASE_OUTSTATE_ASYNC)
        return;
    self->outstate = MB_PIPEBASE_OUTSTATE_IDLE;
    mb_fsm_event_start (&self->out, MB_PIPE_OUT);
}

void mb_pipebase_getopt (struct mb_pipebase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    mb_sock_getopt_inner (self->sock, level, option, optval, optvallen);
}

int mb_pipebase_ispeer (struct mb_pipebase *self, int socktype)
{
    return mb_sock_ispeer (self->sock, socktype);
}

void mb_pipe_setdata (struct mb_pipe *self, void *data)
{
    ((struct mb_pipebase *)self)->data = data;
}

void *mb_pipe_getdata (struct mb_pipe *self)
{
    return ((struct mb_pipebase *)self)->data;
}

int mb_pipe_send (struct mb_pipe *self, struct mb_msg *msg)
{
    struct mb_pipebase *base = (struct mb_pipebase *)self;
    return base->vfptr->send (base, msg);
}

int mb_pipe_recv (struct mb_pipe *self, struct mb_msg *msg)
{
    struct mb_pipebase *base = (struct mb_pipebase *)self;
    return base->vfptr->recv (base, msg);
}

void mb_pipe_getopt (struct mb_pipe *self, int level, int option,
    void *optval, size_t *optvallen)
{
    mb_pipebase_getopt ((struct mb_pipebase *)self, level, option,
        optval, optvallen);
}
