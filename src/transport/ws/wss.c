#include "../../transport.h"
#include "../../core/ep.h"
#include "bwss.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_wss.h>

static int mb_wss_bind (struct mb_ep *ep)
{
    return mb_bwss_create (ep);
}

static int mb_wss_connect (struct mb_ep *ep)
{
    return mb_cwss_create (ep);
}

const struct mb_transport mb_wss_transport = {
    "wss",
    MB_WSS,
    NULL,
    NULL,
    mb_wss_bind,
    mb_wss_connect,
    NULL,
};
