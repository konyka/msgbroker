#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_ws_basic (void)
{
    int s1, s2, rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9100");
    assert (rc >= 0);

    rc = mb_connect (s2, "ws://127.0.0.1:9100");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    mb_close (s2);
    mb_close (s1);

    printf ("  ws_basic: OK\n");
}

static void test_ws_bidir (void)
{
    int s1, s2, rc;
    char buf[64];
    int i;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9101");
    assert (rc >= 0);
    rc = mb_connect (s2, "ws://127.0.0.1:9101");
    assert (rc >= 0);

    for (i = 0; i < 100; i++) {
        char send_buf[32];
        int len = snprintf (send_buf, sizeof (send_buf), "msg_%d", i);
        rc = mb_send (s2, send_buf, (size_t) len, 0);
        assert (rc == len);
        rc = mb_recv (s1, buf, sizeof (buf), 0);
        assert (rc == len);
        assert (memcmp (buf, send_buf, (size_t) len) == 0);
    }

    mb_close (s2);
    mb_close (s1);

    printf ("  ws_bidir: OK\n");
}

static void test_ws_large_msg (void)
{
    int s1, s2, rc;
    char send_buf[256];
    char recv_buf[256];
    size_t i;

    for (i = 0; i < sizeof (send_buf); i++)
        send_buf[i] = (char) ('A' + (i % 26));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9102");
    assert (rc >= 0);
    rc = mb_connect (s2, "ws://127.0.0.1:9102");
    assert (rc >= 0);

    rc = mb_send (s2, send_buf, sizeof (send_buf), 0);
    assert (rc == (int) sizeof (send_buf));
    rc = mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    assert (rc == (int) sizeof (send_buf));
    assert (memcmp (recv_buf, send_buf, sizeof (send_buf)) == 0);

    mb_close (s2);
    mb_close (s1);
    printf ("  ws_large_msg: OK\n");
}

/*  8-byte WS lengths >= 2^31 must not cast into negative payload_len. */
static void test_ws_payload_len_int_overflow (void)
{
    int s1;
    int rc;
    int unlimited = -1;
    int rcvtimeo = 2000;
    int raw;
    char buf[16];
    char req[256];
    char resp[512];
    uint8_t frame[14];
    struct sockaddr_in sa;
    ssize_t n;
    size_t got = 0;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVMAXSIZE, &unlimited,
        sizeof (unlimited));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVTIMEO, &rcvtimeo,
        sizeof (rcvtimeo));
    assert (rc == 0);

    rc = mb_bind (s1, "ws://127.0.0.1:9103");
    assert (rc >= 0);
    usleep (50000);

    raw = socket (AF_INET, SOCK_STREAM, 0);
    assert (raw >= 0);
    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons (9103);
    assert (inet_pton (AF_INET, "127.0.0.1", &sa.sin_addr) == 1);
    rc = connect (raw, (struct sockaddr *) &sa, sizeof (sa));
    assert (rc == 0);

    snprintf (req, sizeof (req),
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:9103\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n");
    n = send (raw, req, strlen (req), 0);
    assert (n == (ssize_t) strlen (req));

    got = 0;
    while (got < sizeof (resp) - 1) {
        n = recv (raw, resp + got, sizeof (resp) - 1 - got, 0);
        if (n <= 0)
            break;
        got += (size_t) n;
        resp[got] = '\0';
        if (strstr (resp, "\r\n\r\n"))
            break;
    }
    assert (strstr (resp, "101") != NULL);

    /* Masked PING, 8-byte length = 0x80000000 (overflows signed int). */
    memset (frame, 0, sizeof (frame));
    frame[0] = 0x89; /* FIN + PING */
    frame[1] = 0x80 | 127; /* MASK + 8-byte length */
    frame[2] = frame[3] = frame[4] = frame[5] = 0;
    frame[6] = 0x80;
    frame[7] = frame[8] = frame[9] = 0;
    frame[10] = 0x11;
    frame[11] = 0x22;
    frame[12] = 0x33;
    frame[13] = 0x44;
    n = send (raw, frame, sizeof (frame), 0);
    assert (n == (ssize_t) sizeof (frame));

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc < 0);
    assert (mb_errno () == EMSGSIZE || mb_errno () == EPROTO ||
        mb_errno () == ECONNRESET);

    close (raw);
    mb_close (s1);
    printf ("  ws_payload_len_int_overflow: OK\n");
}

int main (void)
{
    printf ("test_ws:\n");
    test_ws_basic ();
    test_ws_bidir ();
    test_ws_large_msg ();
    test_ws_payload_len_int_overflow ();
    printf ("test_ws: PASSED\n");
    return 0;
}
