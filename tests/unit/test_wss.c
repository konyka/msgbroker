#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

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

struct fake_wss_arg {
    int listen_fd;
    const char *cert;
    const char *key;
    const char *resp;
};

/* Fake TLS WS handshake responder (resp chosen by caller). */
static void *fake_wss_handshake_resp (void *arg)
{
    struct fake_wss_arg *a = (struct fake_wss_arg *) arg;
    SSL_CTX *ctx;
    SSL *ssl;
    int fd;
    char req[4096];
    size_t pos = 0;
    const char *resp = a->resp;

    ctx = SSL_CTX_new (TLS_server_method ());
    if (!ctx)
        return NULL;
    SSL_CTX_set_min_proto_version (ctx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file (ctx, a->cert, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file (ctx, a->key, SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free (ctx);
        return NULL;
    }

    fd = accept (a->listen_fd, NULL, NULL);
    if (fd < 0) {
        SSL_CTX_free (ctx);
        return NULL;
    }

    ssl = SSL_new (ctx);
    if (!ssl) {
        close (fd);
        SSL_CTX_free (ctx);
        return NULL;
    }
    SSL_set_fd (ssl, fd);
    if (SSL_accept (ssl) != 1) {
        SSL_free (ssl);
        close (fd);
        SSL_CTX_free (ctx);
        return NULL;
    }

    while (pos < sizeof (req) - 1) {
        int nr = SSL_read (ssl, req + pos, 1);
        if (nr <= 0)
            break;
        pos += (size_t) nr;
        req[pos] = '\0';
        if (pos >= 4 && memcmp (req + pos - 4, "\r\n\r\n", 4) == 0)
            break;
    }

    (void) SSL_write (ssl, resp, (int) strlen (resp));
    usleep (200000);
    SSL_shutdown (ssl);
    SSL_free (ssl);
    close (fd);
    SSL_CTX_free (ctx);
    return NULL;
}

static void test_wss_reject_missing_accept (void)
{
    int listen_fd;
    int s2;
    int rc;
    int ivl = 0;
    int verify = 0;
    struct sockaddr_in sa;
    socklen_t salen = sizeof (sa);
    pthread_t thr;
    struct fake_wss_arg arg;
    uint16_t port;

    system ("openssl req -x509 -newkey rsa:2048 "
        "-keyout /tmp/mb_test_wss_noacc_key.pem "
        "-out /tmp/mb_test_wss_noacc_cert.pem "
        "-days 1 -nodes -subj '/CN=localhost' 2>/dev/null");

    listen_fd = socket (AF_INET, SOCK_STREAM, 0);
    assert (listen_fd >= 0);
    {
        int on = 1;
        setsockopt (listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
    }
    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    sa.sin_port = 0;
    rc = bind (listen_fd, (struct sockaddr *) &sa, sizeof (sa));
    assert (rc == 0);
    rc = getsockname (listen_fd, (struct sockaddr *) &sa, &salen);
    assert (rc == 0);
    port = ntohs (sa.sin_port);
    rc = listen (listen_fd, 1);
    assert (rc == 0);

    arg.listen_fd = listen_fd;
    arg.cert = "/tmp/mb_test_wss_noacc_cert.pem";
    arg.key = "/tmp/mb_test_wss_noacc_key.pem";
    arg.resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "\r\n";
    rc = pthread_create (&thr, NULL, fake_wss_handshake_resp, &arg);
    assert (rc == 0);

    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    rc = mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify,
        sizeof (verify));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl,
        sizeof (ivl));
    assert (rc == 0);

    {
        char url[64];
        snprintf (url, sizeof (url), "wss://127.0.0.1:%u",
            (unsigned) port);
        rc = mb_connect (s2, url);
    }
    assert (rc < 0);

    pthread_join (thr, NULL);
    close (listen_fd);
    mb_close (s2);
    printf ("  test_wss_reject_missing_accept: PASSED\n");
}

static void test_wss_reject_bad_accept (void)
{
    int listen_fd;
    int s2;
    int rc;
    int ivl = 0;
    int verify = 0;
    struct sockaddr_in sa;
    socklen_t salen = sizeof (sa);
    pthread_t thr;
    struct fake_wss_arg arg;
    uint16_t port;

    system ("openssl req -x509 -newkey rsa:2048 "
        "-keyout /tmp/mb_test_wss_badacc_key.pem "
        "-out /tmp/mb_test_wss_badacc_cert.pem "
        "-days 1 -nodes -subj '/CN=localhost' 2>/dev/null");

    listen_fd = socket (AF_INET, SOCK_STREAM, 0);
    assert (listen_fd >= 0);
    {
        int on = 1;
        setsockopt (listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
    }
    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    sa.sin_port = 0;
    rc = bind (listen_fd, (struct sockaddr *) &sa, sizeof (sa));
    assert (rc == 0);
    rc = getsockname (listen_fd, (struct sockaddr *) &sa, &salen);
    assert (rc == 0);
    port = ntohs (sa.sin_port);
    rc = listen (listen_fd, 1);
    assert (rc == 0);

    arg.listen_fd = listen_fd;
    arg.cert = "/tmp/mb_test_wss_badacc_cert.pem";
    arg.key = "/tmp/mb_test_wss_badacc_key.pem";
    arg.resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: dGVzdA==\r\n"
        "\r\n";
    rc = pthread_create (&thr, NULL, fake_wss_handshake_resp, &arg);
    assert (rc == 0);

    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    rc = mb_setsockopt (s2, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify,
        sizeof (verify));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl,
        sizeof (ivl));
    assert (rc == 0);

    {
        char url[64];
        snprintf (url, sizeof (url), "wss://127.0.0.1:%u",
            (unsigned) port);
        rc = mb_connect (s2, url);
    }
    assert (rc < 0);

    pthread_join (thr, NULL);
    close (listen_fd);
    mb_close (s2);
    printf ("  test_wss_reject_bad_accept: PASSED\n");
}

int main (void)
{
    printf ("WSS (WebSocket Secure) Tests:\n");

    test_wss_with_certs ();
    test_wss_bidirectional ();
    test_wss_reject_missing_accept ();
    test_wss_reject_bad_accept ();

    printf ("\nAll WSS tests PASSED\n");
    return 0;
}
