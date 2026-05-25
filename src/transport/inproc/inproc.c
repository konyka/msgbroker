#include "../../transport.h"
#include "../../core/ep.h"
#include "ins.h"
#include "binproc.h"
#include "cinproc.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_inproc.h>

static void mb_inproc_init (void)
{
    mb_ins_init ();
}

static void mb_inproc_term (void)
{
    mb_ins_term ();
}

static int mb_inproc_bind (struct mb_ep *ep)
{
    return mb_binproc_create (ep);
}

static int mb_inproc_connect (struct mb_ep *ep)
{
    return mb_cinproc_create (ep);
}

const struct mb_transport mb_inproc_transport = {
    "inproc",
    MB_INPROC,
    mb_inproc_init,
    mb_inproc_term,
    mb_inproc_bind,
    mb_inproc_connect,
    NULL,
};
