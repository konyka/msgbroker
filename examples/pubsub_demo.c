#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    int pub = mb_socket (AF_MB, MB_PUB);
    int sub1 = mb_socket (AF_MB, MB_SUB);
    int sub2 = mb_socket (AF_MB, MB_SUB);

    mb_bind (pub, "tcp://127.0.0.1:9003");
    mb_connect (sub1, "tcp://127.0.0.1:9003");
    mb_connect (sub2, "tcp://127.0.0.1:9003");
    /* Empty topic = subscribe to all (nanomsg semantics). */
    mb_setsockopt (sub1, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    mb_setsockopt (sub2, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    usleep (100000);

    mb_send (pub, "NEWS", 4, 0);
    printf ("pub broadcast: NEWS\n");

    char buf[64];
    int n;

    n = mb_recv (sub1, buf, sizeof (buf), 0);
    if (n > 0) printf ("sub1 recv: %.*s\n", n, buf);

    n = mb_recv (sub2, buf, sizeof (buf), 0);
    if (n > 0) printf ("sub2 recv: %.*s\n", n, buf);

    mb_close (sub2);
    mb_close (sub1);
    mb_close (pub);
    return 0;
}
