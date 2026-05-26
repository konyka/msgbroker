#include "../../transport.h"
#include "../../core/ep.h"
#include "btls.h"
#include "ctls.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_tls.h>
#include <openssl/ssl.h>

static void mb_tls_init (void)
{
    SSL_library_init ();
    SSL_load_error_strings ();
    OpenSSL_add_all_algorithms ();
}

static void mb_tls_term (void)
{
}

static int mb_tls_bind (struct mb_ep *ep)
{
    return mb_btls_create (ep);
}

static int mb_tls_connect (struct mb_ep *ep)
{
    return mb_ctls_create (ep);
}

const struct mb_transport mb_tls_transport = {
    "tls",
    MB_TLS,
    mb_tls_init,
    mb_tls_term,
    mb_tls_bind,
    mb_tls_connect,
    NULL,
};
