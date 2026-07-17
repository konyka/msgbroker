#ifndef MB_TRANSPORT_H_INCLUDED
#define MB_TRANSPORT_H_INCLUDED

#include "aio/fsm.h"
#include "utils/list.h"
#include "memory/msg.h"

#include <stddef.h>

struct mb_sock;
struct mb_ep;

/******************************************************************************/
/*  Option set — transport-specific socket options.                           */
/******************************************************************************/

struct mb_optset;

struct mb_optset_vfptr {
    void (*destroy) (struct mb_optset *self);
    int (*setopt) (struct mb_optset *self, int option, const void *optval,
        size_t optvallen);
    int (*getopt) (struct mb_optset *self, int option, void *optval,
        size_t *optvallen);
};

struct mb_optset {
    const struct mb_optset_vfptr *vfptr;
};

/******************************************************************************/
/*  Endpoint operations — provided by transport implementations.              */
/******************************************************************************/

struct mb_ep_ops {
    void (*stop) (void *transport);
    void (*destroy) (void *transport);
    void (*on_disconnect) (void *transport);
};

/******************************************************************************/
/*  Pipe base — one logical connection within an endpoint.                    */
/******************************************************************************/

#define MB_PIPEBASE_RELEASE 1
#define MB_PIPEBASE_PARSED  2

struct mb_pipebase;

struct mb_pipebase_vfptr {
    int (*send) (struct mb_pipebase *self, struct mb_msg *msg);
    int (*recv) (struct mb_pipebase *self, struct mb_msg *msg);
    /* Non-destructive: 1 if a message is queued for recv. */
    int (*has_msg) (struct mb_pipebase *self);
};

struct mb_ep_options {
    int sndprio;
    int rcvprio;
    int ipv4only;
};

struct mb_pipebase {
    struct mb_fsm fsm;
    const struct mb_pipebase_vfptr *vfptr;
    uint8_t state;
    uint8_t instate;
    uint8_t outstate;
    struct mb_sock *sock;
    void *data;
    struct mb_fsm_event in;
    struct mb_fsm_event out;
    struct mb_ep_options options;
};

void mb_pipebase_init (struct mb_pipebase *self,
    const struct mb_pipebase_vfptr *vfptr, struct mb_ep *ep);
void mb_pipebase_term (struct mb_pipebase *self);
int mb_pipebase_start (struct mb_pipebase *self);
void mb_pipebase_stop (struct mb_pipebase *self);
void mb_pipebase_received (struct mb_pipebase *self);
void mb_pipebase_sent (struct mb_pipebase *self);
void mb_pipebase_getopt (struct mb_pipebase *self, int level, int option,
    void *optval, size_t *optvallen);
int mb_pipebase_ispeer (struct mb_pipebase *self, int socktype);

/******************************************************************************/
/*  Transport class — registered globally, provides bind/connect.             */
/******************************************************************************/

#define MB_MAX_TRANSPORT 6

struct mb_transport {
    const char *name;
    int id;
    void (*init) (void);
    void (*term) (void);
    int (*bind) (struct mb_ep *ep);
    int (*connect) (struct mb_ep *ep);
    struct mb_optset *(*optset) (void);
};

void mb_ep_tran_setup (struct mb_ep *ep, const struct mb_ep_ops *ops,
    void *transport);
void mb_ep_stopped (struct mb_ep *ep);
struct mb_ctx *mb_ep_getctx (struct mb_ep *ep);
const char *mb_ep_getaddr (struct mb_ep *ep);
void mb_ep_getopt (struct mb_ep *ep, int level, int option,
    void *optval, size_t *optvallen);
int mb_ep_ispeer (struct mb_ep *ep, int socktype);
void mb_ep_set_error (struct mb_ep *ep, int errnum);
void mb_ep_clear_error (struct mb_ep *ep);
void mb_ep_stat_increment (struct mb_ep *ep, int name, int increment);

#endif
