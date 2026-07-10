/*
 * test_reconnect_disc.c - Disconnect-during-operation auto-reconnect tests.
 *
 * Validates the on_error/on_disconnect mechanism: when an established
 * connection breaks mid-stream, the session layer (sipc/stls/sws) reports
 * the error, the connect-side transport (cipc/ctcp/ctls/cws/cwss) tears the
 * dead session down and restarts its reconnect loop, so messaging resumes
 * once the server comes back. This is distinct from initial-connect-failure
 * reconnect (covered by test_reconnect.c) which only exercises the reconnect
 * loop before a first connection is ever made.
 *
 * Covered connect transports: tcp, ipc, ws, tls, wss.
 *
 * Verification uses CHECK(), not assert(), because this suite is also built
 * in Release (-DNDEBUG) where assert(X) expands to ((void)0) and would make
 * the test pass vacuously without actually exercising the checks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tls.h>

#define CERT_FILE "/tmp/mb_test_dr_cert.pem"
#define KEY_FILE  "/tmp/mb_test_dr_key.pem"

/* NDEBUG-safe: returns 1 (failure) from the caller on a broken condition,
 * unlike assert() which is compiled out under -DNDEBUG. */
#define CHECK(cond) \
    do { if (!(cond)) { \
        printf ("    FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } } while (0)

static int generate_self_signed_cert (void)
{
    return system ("openssl req -x509 -newkey rsa:2048 -keyout " KEY_FILE
        " -out " CERT_FILE
        " -days 1 -nodes -subj '/CN=localhost' 2>/dev/null");
}

static void cleanup_cert_files (void)
{
    unlink (CERT_FILE);
    unlink (KEY_FILE);
}

static void setup_tls_server (int s)
{
    int verify = 0;
    const char *cert = CERT_FILE;
    const char *key = KEY_FILE;
    mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_CERT, cert, strlen (cert) + 1);
    mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_KEY, key, strlen (key) + 1);
    mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify, sizeof (verify));
}

static void setup_tls_client (int s)
{
    int verify = 0;
    mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify, sizeof (verify));
}

static int bind_server (int s, int setup_tls, const char *addr, int rcv_tmo)
{
    int rc;

    if (setup_tls)
        setup_tls_server (s);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO,
        &rcv_tmo, sizeof (rcv_tmo));
    if (rc != 0)
        return rc;

    return mb_bind (s, addr);
}

static int connect_client (int s, int setup_tls, const char *addr,
    int ivl, int rcv_tmo)
{
    int rc;

    if (setup_tls)
        setup_tls_client (s);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL,
        &ivl, sizeof (ivl));
    if (rc != 0)
        return rc;

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO,
        &rcv_tmo, sizeof (rcv_tmo));
    if (rc != 0)
        return rc;

    return mb_connect (s, addr);
}

/* Returns 0 on success, 1 on failure. */
static int run_disconnect_reconnect (int setup_tls, const char *addr,
    int srv_rcv_tmo)
{
    int s_srv, s_cli, rc, i;
    char buf[64];

    s_srv = mb_socket (AF_MB, MB_PAIR);
    CHECK (s_srv >= 0);
    rc = bind_server (s_srv, setup_tls, addr, srv_rcv_tmo);
    CHECK (rc >= 0);

    s_cli = mb_socket (AF_MB, MB_PAIR);
    CHECK (s_cli >= 0);
    rc = connect_client (s_cli, setup_tls, addr, 100, 400);
    CHECK (rc >= 0);

    usleep (250000);

    rc = mb_send (s_cli, "BEFORE", 6, 0);
    CHECK (rc == 6);
    rc = mb_recv (s_srv, buf, sizeof (buf), 0);
    CHECK (rc == 6);
    CHECK (memcmp (buf, "BEFORE", 6) == 0);

    mb_close (s_srv);

    /* Trigger break detection: let the FIN/RST arrive, then recv drives the
     * session recv path; the hard error fires on_error -> on_disconnect tears
     * the session down and starts the reconnect loop. Return value is
     * intentionally ignored: -EAGAIN/-ECONNRESET/timeout are all consistent
     * with the pipe being torn down -- the side effect matters. */
    usleep (200000);
    mb_recv (s_cli, buf, sizeof (buf), 0);

    s_srv = mb_socket (AF_MB, MB_PAIR);
    CHECK (s_srv >= 0);
    rc = bind_server (s_srv, setup_tls, addr, srv_rcv_tmo);
    CHECK (rc >= 0);

    /* Poll for reconnect completion: non-blocking send/recv every 50ms. The
     * client's reconnect loop runs every RECONNECT_IVL, so within ~2s the
     * pipe comes back and the round-trip succeeds. (Blocking mb_send/mb_recv
     * would deadlock here: default sndtimeo is -1 and, while no pipe exists,
     * pair_send returns -EAGAIN on every attempt.) */
    rc = -1;
    for (i = 0; i < 40 && rc < 5; i++) {
        mb_send (s_cli, "AFTER", 5, MB_DONTWAIT);
        rc = mb_recv (s_srv, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 5)
            usleep (50000);
    }
    CHECK (rc == 5);
    CHECK (memcmp (buf, "AFTER", 5) == 0);

    mb_close (s_cli);
    mb_close (s_srv);
    return 0;
}

static int test_disconnect_reconnect_tcp (void)
{
    int rc = run_disconnect_reconnect (0, "tcp://127.0.0.1:18901", 400);
    printf ("  disconnect_reconnect_tcp: %s\n", rc ? "FAIL" : "OK");
    return rc;
}

static int test_disconnect_reconnect_ipc (void)
{
    int rc;
    unlink ("/tmp/mb_dr_ipc.sock");
    rc = run_disconnect_reconnect (0, "ipc:///tmp/mb_dr_ipc.sock", 400);
    unlink ("/tmp/mb_dr_ipc.sock");
    printf ("  disconnect_reconnect_ipc: %s\n", rc ? "FAIL" : "OK");
    return rc;
}

static int test_disconnect_reconnect_ws (void)
{
    int rc = run_disconnect_reconnect (0, "ws://127.0.0.1:18902", 400);
    printf ("  disconnect_reconnect_ws: %s\n", rc ? "FAIL" : "OK");
    return rc;
}

static int test_disconnect_reconnect_tls (void)
{
    int rc;
    generate_self_signed_cert ();
    rc = run_disconnect_reconnect (1, "tls://127.0.0.1:18903", 500);
    cleanup_cert_files ();
    printf ("  disconnect_reconnect_tls: %s\n", rc ? "FAIL" : "OK");
    return rc;
}

static int test_disconnect_reconnect_wss (void)
{
    int rc;
    generate_self_signed_cert ();
    rc = run_disconnect_reconnect (1, "wss://127.0.0.1:18904", 600);
    cleanup_cert_files ();
    printf ("  disconnect_reconnect_wss: %s\n", rc ? "FAIL" : "OK");
    return rc;
}

int main (void)
{
    int failed = 0;

    printf ("Disconnect-Reconnect Tests:\n");

    failed |= test_disconnect_reconnect_tcp ();
    failed |= test_disconnect_reconnect_ipc ();
    failed |= test_disconnect_reconnect_ws ();
    failed |= test_disconnect_reconnect_tls ();
    failed |= test_disconnect_reconnect_wss ();

    if (failed) {
        printf ("\nFAILURES detected\n");
        return 1;
    }
    printf ("\nAll disconnect-reconnect tests PASSED\n");
    return 0;
}
