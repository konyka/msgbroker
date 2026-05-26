#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tls.h>

static void test_wss_with_certs (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    system ("openssl req -x509 -newkey rsa:2048 "
        "-keyout /tmp/mb_test_wss_key.pem "
        "-out /tmp/mb_test_wss_cert.pem "
        "-days 1 -nodes -subj '/CN=localhost' 2>/dev/null");

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_CERT,
        "/tmp/mb_test_wss_cert.pem", 25);
    mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_KEY,
        "/tmp/mb_test_wss_key.pem", 24);

    {
        int verify = 0;
        mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY,
            &verify, sizeof (verify));
    }

    rc = mb_bind (s1, "wss://127.0.0.1:18897");
    assert (rc >= 0);

    usleep (200000);

    rc = mb_connect (s2, "wss://127.0.0.1:18897");
    assert (rc >= 0);

    usleep (200000);

    rc = mb_send (s2, "WSS_OK", 6, 0);
    assert (rc == 6);

    usleep (200000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 6);
    assert (memcmp (buf, "WSS_OK", 6) == 0);

    rc = mb_send (s1, "REPLY", 5, 0);
    assert (rc == 5);

    usleep (200000);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "REPLY", 5) == 0);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_wss_with_certs: PASSED\n");
}

static void test_wss_bidirectional (void)
{
    int s1, s2;
    int rc;
    char buf[64];
    int i;

    system ("openssl req -x509 -newkey rsa:2048 "
        "-keyout /tmp/mb_test_wss_key2.pem "
        "-out /tmp/mb_test_wss_cert2.pem "
        "-days 1 -nodes -subj '/CN=localhost' 2>/dev/null");

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_CERT,
        "/tmp/mb_test_wss_cert2.pem", 26);
    mb_setsockopt (s1, MB_TLS, MB_TLS_CONFIG_KEY,
        "/tmp/mb_test_wss_key2.pem", 25);

    {
        int verify = 0;
        mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY,
            &verify, sizeof (verify));
    }

    rc = mb_bind (s1, "wss://127.0.0.1:18898");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_connect (s2, "wss://127.0.0.1:18898");
    assert (rc >= 0);

    usleep (200000);

    for (i = 0; i < 50; i++) {
        char send_buf[32];
        int len = snprintf (send_buf, sizeof (send_buf), "msg_%d", i);

        rc = mb_send (s2, send_buf, len, 0);
        assert (rc == len);

        usleep (5000);

        rc = mb_recv (s1, buf, sizeof (buf), 0);
        assert (rc == len);
        assert (memcmp (buf, send_buf, len) == 0);
    }

    mb_close (s1);
    mb_close (s2);

    printf ("  test_wss_bidirectional: PASSED\n");
}

int main (void)
{
    printf ("WSS (WebSocket Secure) Tests:\n");

    test_wss_with_certs ();
    test_wss_bidirectional ();

    printf ("\nAll WSS tests PASSED\n");
    return 0;
}
