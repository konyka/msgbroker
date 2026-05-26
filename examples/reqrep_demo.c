#include <msgbroker/mb.h>
#include <msgbroker/mb_reqrep.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main (void)
{
    int req = mb_socket (AF_MB, MB_REQ);
    int rep = mb_socket (AF_MB, MB_REP);

    mb_bind (rep, "tcp://127.0.0.1:9002");
    mb_connect (req, "tcp://127.0.0.1:9002");
    usleep (100000);

    mb_send (req, "PING", 4, 0);
    printf ("req sent: PING\n");

    char buf[64];
    int n = mb_recv (rep, buf, sizeof (buf), 0);
    if (n > 0) {
        buf[n] = '\0';
        printf ("rep recv: %s\n", buf);
    }

    mb_send (rep, "PONG", 4, 0);
    printf ("rep sent: PONG\n");

    n = mb_recv (req, buf, sizeof (buf), 0);
    if (n > 0) {
        buf[n] = '\0';
        printf ("req recv: %s\n", buf);
    }

    mb_close (req);
    mb_close (rep);
    return 0;
}
