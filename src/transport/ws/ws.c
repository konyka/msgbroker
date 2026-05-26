#include "../../transport.h"
#include "../../core/ep.h"
#include "bws.h"
#include "cws.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_ws.h>

static int mb_ws_bind (struct mb_ep *ep)
{
    return mb_bws_create (ep);
}

static int mb_ws_connect (struct mb_ep *ep)
{
    return mb_cws_create (ep);
}

const struct mb_transport mb_ws_transport = {
    "ws",
    MB_WS,
    NULL,
    NULL,
    mb_ws_bind,
    mb_ws_connect,
    NULL,
};
