#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    int s1;
    int s2;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    if (s1 < 0) {
        printf ("mb_socket failed: %s\n", mb_strerror (mb_errno ()));
        return 1;
    }

    s2 = mb_socket (AF_MB, MB_PAIR);
    if (s2 < 0) {
        printf ("mb_socket failed: %s\n", mb_strerror (mb_errno ()));
        mb_close (s1);
        return 1;
    }

    mb_bind (s1, "tcp://127.0.0.1:9000");
    mb_connect (s2, "tcp://127.0.0.1:9000");
    usleep (100000);

    mb_send (s2, "HELLO", 5, 0);
    printf ("sent: HELLO\n");

    int n = mb_recv (s1, buf, sizeof (buf), 0);
    if (n >= 0) {
        buf[n] = '\0';
        printf ("recv: %s\n", buf);
    }

    mb_close (s2);
    mb_close (s1);
    return 0;
}
