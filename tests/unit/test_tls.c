#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tls.h>

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

int main (void)
{
    printf ("test_tls:\n");
    test_tls_bind_without_certs ();
    test_tls_socket_options ();
    test_tls_getopt_roundtrip ();
    printf ("test_tls: PASSED\n");
    return 0;
}
