#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    int push = mb_socket (AF_MB, MB_PUSH);
    int pull = mb_socket (AF_MB, MB_PULL);

    mb_bind (pull, "tcp://127.0.0.1:9001");
    mb_connect (push, "tcp://127.0.0.1:9001");
    usleep (100000);

    const char *messages[] = { "task1", "task2", "task3", "task4", "task5" };
    int i;

    for (i = 0; i < 5; i++) {
        mb_send (push, messages[i], 5, 0);
        printf ("sent: %s\n", messages[i]);
    }

    for (i = 0; i < 5; i++) {
        char buf[64];
        int n = mb_recv (pull, buf, sizeof (buf), 0);
        if (n > 0) {
            buf[n] = '\0';
            printf ("recv: %s\n", buf);
        }
    }

    mb_close (push);
    mb_close (pull);
    return 0;
}
