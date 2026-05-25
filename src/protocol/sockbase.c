#include "../protocol.h"
#include "../core/sock.h"
#include "../utils/alloc.h"

#include <msgbroker/mb.h>

void mb_sockbase_init (struct mb_sockbase *self,
    const struct mb_sockbase_vfptr *vfptr, void *hint)
{
    self->vfptr = vfptr;
    self->sock = (struct mb_sock *) hint;
}

void mb_sockbase_term (struct mb_sockbase *self)
{
    (void) self;
}

void mb_sockbase_stopped (struct mb_sockbase *self)
{
    mb_sock_stopped (self->sock);
}

struct mb_ctx *mb_sockbase_getctx (struct mb_sockbase *self)
{
    return mb_sock_getctx (self->sock);
}

int mb_sockbase_getopt (struct mb_sockbase *self, int option,
    void *optval, size_t *optvallen)
{
    return mb_sock_getopt (self->sock, MB_SOL_SOCKET, option, optval, optvallen);
}

void mb_sockbase_stat_increment (struct mb_sockbase *self, int name,
    int increment)
{
    mb_sock_stat_increment (self->sock, name, increment);
}
