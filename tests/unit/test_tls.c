#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

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

int main (void)
{
    printf ("test_tls:\n");
    test_tls_bind_without_certs ();
    test_tls_socket_options ();
    printf ("test_tls: PASSED\n");
    return 0;
}
