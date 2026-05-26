#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tls.h>

#define CERT_FILE "/tmp/mb_test_cert.pem"
#define KEY_FILE  "/tmp/mb_test_key.pem"

static void generate_self_signed_cert (void)
{
    system ("openssl req -x509 -newkey rsa:2048 -keyout " KEY_FILE
        " -out " CERT_FILE
        " -days 1 -nodes -subj '/CN=localhost' 2>/dev/null");
}

static void cleanup_cert_files (void)
{
    unlink (CERT_FILE);
    unlink (KEY_FILE);
}

static void test_tls_cert_sendrecv (void)
{
    int s1, s2, rc;
    char buf[64];
    const char *cert_path = CERT_FILE;
    const char *key_path = KEY_FILE;

    generate_self_signed_cert ();

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_CERT,
        cert_path, strlen (cert_path) + 1);
    assert (rc == 0);

    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_KEY,
        key_path, strlen (key_path) + 1);
    assert (rc == 0);

    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);

    rc = mb_bind (s1, "tls://*:5560");
    assert (rc >= 0);

    rc = mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);

    rc = mb_connect (s2, "tls://127.0.0.1:5560");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO TLS", 9, 0);
    assert (rc == 9);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 9);
    assert (memcmp (buf, "HELLO TLS", 9) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    mb_close (s2);
    mb_close (s1);

    cleanup_cert_files ();
    printf ("  tls_cert_sendrecv: OK\n");
}

static void test_tls_cert_bidir (void)
{
    int s1, s2, rc;
    char buf[64];
    int i;

    generate_self_signed_cert ();

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_CERT,
        CERT_FILE, strlen (CERT_FILE) + 1);
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_KEY,
        KEY_FILE, strlen (KEY_FILE) + 1);
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);

    rc = mb_bind (s1, "tls://*:5561");
    assert (rc >= 0);

    rc = mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);

    rc = mb_connect (s2, "tls://127.0.0.1:5561");
    assert (rc >= 0);

    for (i = 0; i < 100; i++) {
        char send_buf[32];
        int len = snprintf (send_buf, sizeof (send_buf), "tls_%d", i);
        rc = mb_send (s2, send_buf, (size_t) len, 0);
        assert (rc == len);
        rc = mb_recv (s1, buf, sizeof (buf), 0);
        assert (rc == len);
        assert (memcmp (buf, send_buf, (size_t) len) == 0);
    }

    mb_close (s2);
    mb_close (s1);

    cleanup_cert_files ();
    printf ("  tls_cert_bidir: OK\n");
}

static void test_tls_no_cert_connect_fails (void)
{
    int s1, s2, rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);

    rc = mb_bind (s1, "tls://*:5562");
    fprintf (stderr, "  no-cert bind rc=%d errno=%d\n", rc, rc < 0 ? mb_errno () : 0);
    assert (rc >= 0);

    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    rc = mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY,
        &(int){0}, sizeof (int));
    assert (rc == 0);
    {
        int ivl = 0;
        mb_setsockopt (s2, MB_SOL_SOCKET, MB_RECONNECT_IVL,
            &ivl, sizeof (ivl));
    }

    rc = mb_connect (s2, "tls://127.0.0.1:5562");
    assert (rc < 0);

    mb_close (s2);
    mb_close (s1);
    printf ("  tls_no_cert_connect_fails: OK\n");
}

int main (void)
{
    printf ("test_tls_cert:\n");
    test_tls_no_cert_connect_fails ();
    test_tls_cert_sendrecv ();
    test_tls_cert_bidir ();
    printf ("test_tls_cert: PASSED\n");
    return 0;
}
