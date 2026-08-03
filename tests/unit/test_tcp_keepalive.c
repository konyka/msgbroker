#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_tcp.h>

static void test_tcp_keepalive (void)
{
    int s;
    int value;
    size_t len;
    int rc;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    value = 1;
    rc = mb_setsockopt (s, MB_TCP, MB_TCP_KEEPALIVE, &value, sizeof (value));
    assert (rc == 0);

    value = 0;
    len = sizeof (value);
    rc = mb_getsockopt (s, MB_TCP, MB_TCP_KEEPALIVE, &value, &len);
    assert (rc == 0);
    assert (len == sizeof (value));
    assert (value == 1);

    rc = mb_close (s);
    assert (rc == 0);
    printf ("  test_tcp_keepalive: PASSED\n");
}

int main (void)
{
    printf ("test_tcp_keepalive:\n");
    test_tcp_keepalive ();
    return 0;
}
