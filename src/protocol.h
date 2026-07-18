#ifndef MB_PROTOCOL_H_INCLUDED
#define MB_PROTOCOL_H_INCLUDED

#include "memory/msg.h"
#include "utils/list.h"

#include <stddef.h>
#include <stdint.h>

struct mb_ctx;

/******************************************************************************/
/*  Pipe — protocol-level interface to message flow.                          */
/******************************************************************************/

#define MB_PIPE_RELEASE 1
#define MB_PIPE_PARSED  2

#define MB_PIPE_IN  33987
#define MB_PIPE_OUT 33988

struct mb_pipe;

void mb_pipe_setdata (struct mb_pipe *self, void *data);
void *mb_pipe_getdata (struct mb_pipe *self);
int mb_pipe_send (struct mb_pipe *self, struct mb_msg *msg);
int mb_pipe_recv (struct mb_pipe *self, struct mb_msg *msg);
int mb_pipe_has_msg (struct mb_pipe *self);
int mb_pipe_can_send (struct mb_pipe *self);
void mb_pipe_getopt (struct mb_pipe *self, int level, int option,
    void *optval, size_t *optvallen);

/******************************************************************************/
/*  Socket base — protocol-level socket interface.                            */
/******************************************************************************/

#define MB_SOCKBASE_EVENT_IN  1
#define MB_SOCKBASE_EVENT_OUT 2

struct mb_sockbase;

struct mb_sockbase_vfptr {
    void (*stop) (struct mb_sockbase *self);
    void (*destroy) (struct mb_sockbase *self);
    int (*add) (struct mb_sockbase *self, struct mb_pipe *pipe);
    void (*rm) (struct mb_sockbase *self, struct mb_pipe *pipe);
    void (*in) (struct mb_sockbase *self, struct mb_pipe *pipe);
    void (*out) (struct mb_sockbase *self, struct mb_pipe *pipe);
    int (*events) (struct mb_sockbase *self);
    int (*send) (struct mb_sockbase *self, struct mb_msg *msg);
    int (*recv) (struct mb_sockbase *self, struct mb_msg *msg);
    int (*setopt) (struct mb_sockbase *self, int level, int option,
        const void *optval, size_t optvallen);
    int (*getopt) (struct mb_sockbase *self, int level, int option,
        void *optval, size_t *optvallen);
};

struct mb_sockbase {
    const struct mb_sockbase_vfptr *vfptr;
    struct mb_sock *sock;
};

void mb_sockbase_init (struct mb_sockbase *self,
    const struct mb_sockbase_vfptr *vfptr, void *hint);
void mb_sockbase_term (struct mb_sockbase *self);
void mb_sockbase_stopped (struct mb_sockbase *self);
struct mb_ctx *mb_sockbase_getctx (struct mb_sockbase *self);
int mb_sockbase_getopt (struct mb_sockbase *self, int option,
    void *optval, size_t *optvallen);
void mb_sockbase_stat_increment (struct mb_sockbase *self, int name,
    int increment);

/******************************************************************************/
/*  Socket type — factory for protocol sockets.                               */
/******************************************************************************/

#define MB_SOCKTYPE_FLAG_NORECV 1
#define MB_SOCKTYPE_FLAG_NOSEND 2
#define MB_SOCKTYPE_FLAG_RAW    4

struct mb_socktype {
    int domain;
    int protocol;
    int flags;
    int (*create) (void *hint, struct mb_sockbase **sockbase);
    int (*ispeer) (int socktype);
};

#endif
