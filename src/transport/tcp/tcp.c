#include "../../transport.h"
#include "../../core/ep.h"
#include "btcp.h"
#include "ctcp.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_tcp.h>

static int mb_tcp_bind (struct mb_ep *ep)
{
    return mb_btcp_create (ep);
}

static int mb_tcp_connect (struct mb_ep *ep)
{
    return mb_ctcp_create (ep);
}

const struct mb_transport mb_tcp_transport = {
    "tcp",
    MB_TCP,
    NULL,
    NULL,
    mb_tcp_bind,
    mb_tcp_connect,
    NULL,
};
