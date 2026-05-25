#include "../../transport.h"
#include "../../core/ep.h"
#include "bipc.h"
#include "cipc.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_ipc.h>

static int mb_ipc_bind (struct mb_ep *ep)
{
    return mb_bipc_create (ep);
}

static int mb_ipc_connect (struct mb_ep *ep)
{
    return mb_cipc_create (ep);
}

const struct mb_transport mb_ipc_transport = {
    "ipc",
    MB_IPC,
    NULL,
    NULL,
    mb_ipc_bind,
    mb_ipc_connect,
    NULL,
};
