#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tls.h>

#define HOSTNAME_CERT "/tmp/mb_test_hostname_cert.pem"
#define HOSTNAME_KEY  "/tmp/mb_test_hostname_key.pem"

/* Generate a self-signed cert whose only identity is DNS:localhost
   (no IP SAN).  Used to exercise hostname verification: a client that
   derives the host from the endpoint address must match "localhost" and
   must reject an IP literal that the cert does not cover. */
static void generate_hostname_cert (void)
{
    int rc;

    rc = system ("openssl req -x509 -newkey rsa:2048 -keyout "
        HOSTNAME_KEY " -out " HOSTNAME_CERT " -days 1 -nodes "
        "-subj '/CN=localhost' "
        "-addext 'subjectAltName=DNS:localhost' 2>/dev/null");
    assert (rc == 0);
}

static void cleanup_hostname_files (void)
{
    unlink (HOSTNAME_CERT);
    unlink (HOSTNAME_KEY);
}

static void configure_server_socket (int s)
{
    int rc;
    int linger = 0;
    int ivl = 0;

    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_CERT,
        HOSTNAME_CERT, strlen (HOSTNAME_CERT) + 1);
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_KEY,
        HOSTNAME_KEY, strlen (HOSTNAME_KEY) + 1);
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL,
        &ivl, sizeof (ivl));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LINGER,
        &linger, sizeof (linger));
    assert (rc == 0);
}

static void configure_verify_client (int s, const char *ca)
{
    int rc;
    int ivl = 0;
    int linger = 0;

    if (ca) {
        rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_CA,
            ca, strlen (ca) + 1);
        assert (rc == 0);
    }
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){1}, sizeof (int));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL,
        &ivl, sizeof (ivl));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LINGER,
        &linger, sizeof (linger));
    assert (rc == 0);
}

static void configure_insecure_client (int s)
{
    int rc;
    int ivl = 0;
    int linger = 0;

    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL,
        &ivl, sizeof (ivl));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_LINGER,
        &linger, sizeof (linger));
    assert (rc == 0);
}

static void test_tls_bind_without_certs (void)
{
    int s1, rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);

    rc = mb_bind (s1, "tls://*:5555");
    if (rc < 0) {
        mb_close (s1);
        printf ("  tls_bind_without_certs: OK (expected failure)\n");
        return;
    }

    mb_close (s1);
    printf ("  tls_bind_without_certs: OK (bind succeeded)\n");
}

static void test_tls_socket_options (void)
{
    int s, rc;
    int val;
    size_t sz;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    sz = sizeof (val);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_DOMAIN, &val, &sz);
    assert (rc == 0);
    assert (val == AF_MB);

    mb_close (s);
    printf ("  tls_socket_options: OK\n");
}

/*  MB_TLS setopt/getopt must round-trip (Phase 145 left TLS getopt unwired). */
static void test_tls_getopt_roundtrip (void)
{
    int s, rc;
    int verify = 1;
    int got = -1;
    size_t sz;
    char path[256];
    const char *cert = "/tmp/mb_test_cert.pem";
    const char *key = "/tmp/mb_test_key.pem";
    const char *ca = "/tmp/mb_test_ca.pem";

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_CERT, cert, strlen (cert));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_KEY, key, strlen (key));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_CA, ca, strlen (ca));
    assert (rc == 0);
    rc = mb_setsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify, sizeof (verify));
    assert (rc == 0);

    sz = sizeof (path);
    rc = mb_getsockopt (s, MB_TLS, MB_TLS_CONFIG_CERT, path, &sz);
    assert (rc == 0);
    assert (strcmp (path, cert) == 0);
    assert (sz == strlen (cert) + 1);

    sz = sizeof (path);
    rc = mb_getsockopt (s, MB_TLS, MB_TLS_CONFIG_KEY, path, &sz);
    assert (rc == 0);
    assert (strcmp (path, key) == 0);

    sz = sizeof (path);
    rc = mb_getsockopt (s, MB_TLS, MB_TLS_CONFIG_CA, path, &sz);
    assert (rc == 0);
    assert (strcmp (path, ca) == 0);

    sz = sizeof (got);
    rc = mb_getsockopt (s, MB_TLS, MB_TLS_CONFIG_VERIFY, &got, &sz);
    assert (rc == 0);
    assert (got == 1);
    assert (sz == sizeof (int));

    sz = sizeof (got);
    rc = mb_getsockopt (s, MB_TLS, 999, &got, &sz);
    assert (rc < 0);
    assert (mb_errno () == ENOPROTOOPT);

    mb_close (s);
    printf ("  test_tls_getopt_roundtrip: OK\n");
}

/* verify=1 + endpoint host matching cert DNS SAN must connect and
   round-trip a payload (proves SNI was sent and hostname verified). */
static void test_tls_verify_hostname_match (void)
{
    int s1, s2, rc;
    char buf[16];

    generate_hostname_cert ();

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    configure_server_socket (s1);
    configure_verify_client (s2, HOSTNAME_CERT);

    rc = mb_bind (s1, "tls://127.0.0.1:5570");
    assert (rc >= 0);

    rc = mb_connect (s2, "tls://localhost:5570");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);
    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    mb_close (s2);
    mb_close (s1);
    cleanup_hostname_files ();
    printf ("  tls_verify_hostname_match: OK\n");
}

/* verify=1 with an IP literal endpoint, while cert only has a DNS
   SAN, must fail the handshake (proves hostname is being checked,
   not just the CA chain). */
static void test_tls_verify_hostname_mismatch (void)
{
    int s1, s2, rc;

    generate_hostname_cert ();

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    configure_server_socket (s1);
    configure_verify_client (s2, HOSTNAME_CERT);

    rc = mb_bind (s1, "tls://127.0.0.1:5571");
    assert (rc >= 0);

    rc = mb_connect (s2, "tls://127.0.0.1:5571");
    fprintf (stderr, "  mismatch connect rc=%d errno=%d\n",
        rc, rc < 0 ? mb_errno () : 0);
    assert (rc < 0);

    mb_close (s2);
    mb_close (s1);
    cleanup_hostname_files ();
    printf ("  tls_verify_hostname_mismatch: OK\n");
}

/* verify=0 with an IP literal endpoint must still connect (the existing
   no-verification path must remain unchanged after the SNI/host
   additions). */
static void test_tls_verify_disabled_unchanged (void)
{
    int s1, s2, rc;
    char buf[16];

    generate_hostname_cert ();

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    configure_server_socket (s1);
    configure_insecure_client (s2);

    rc = mb_bind (s1, "tls://127.0.0.1:5572");
    assert (rc >= 0);

    rc = mb_connect (s2, "tls://127.0.0.1:5572");
    assert (rc >= 0);

    rc = mb_send (s2, "PLAIN", 5, 0);
    assert (rc == 5);
    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "PLAIN", 5) == 0);

    mb_close (s2);
    mb_close (s1);
    cleanup_hostname_files ();
    printf ("  tls_verify_disabled_unchanged: OK\n");
}

int main (void)
{
    printf ("test_tls:\n");
    test_tls_bind_without_certs ();
    test_tls_socket_options ();
    test_tls_getopt_roundtrip ();
    test_tls_verify_hostname_match ();
    test_tls_verify_hostname_mismatch ();
    test_tls_verify_disabled_unchanged ();
    printf ("test_tls: PASSED\n");
    return 0;
}
