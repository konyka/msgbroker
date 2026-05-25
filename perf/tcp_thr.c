/*
    tcp_thr.c — TCP throughput benchmark (PAIR socket)
    Measures messages/sec and MB/sec over TCP loopback.
*/
#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define MSG_SIZE    64
#define MSG_COUNT   100000
#define TCP_ADDR    "tcp://127.0.0.1:9901"

static uint64_t now_ms (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_nsec / 1000000;
}

int main (void)
{
    int s1, s2;
    int i;
    char send_buf[MSG_SIZE];
    char recv_buf[MSG_SIZE];
    uint64_t t_start, t_end;
    double elapsed, msg_rate, mb_rate;

    memset (send_buf, 'C', MSG_SIZE);

    s1 = mb_socket (AF_MB, MB_PAIR);
    s2 = mb_socket (AF_MB, MB_PAIR);
    if (s1 < 0 || s2 < 0) {
        fprintf (stderr, "socket creation failed (err=%d)\n", mb_errno ());
        return 1;
    }

    mb_bind (s1, TCP_ADDR);
    mb_connect (s2, TCP_ADDR);

    usleep (300000);

    for (i = 0; i < 100; i++) {
        mb_send (s2, send_buf, MSG_SIZE, 0);
        mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    }

    printf ("tcp throughput: %d messages of %d bytes\n", MSG_COUNT, MSG_SIZE);

    t_start = now_ms ();
    for (i = 0; i < MSG_COUNT; i++) {
        mb_send (s2, send_buf, MSG_SIZE, 0);
        mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    }
    t_end = now_ms ();

    elapsed = (double) (t_end - t_start) / 1000.0;
    if (elapsed <= 0.0)
        elapsed = 0.001;
    msg_rate = (double) MSG_COUNT / elapsed;
    mb_rate = (double) MSG_COUNT * MSG_SIZE / (1024.0 * 1024.0) / elapsed;

    printf ("  elapsed:    %.3f s\n", elapsed);
    printf ("  msg/sec:    %.0f\n", msg_rate);
    printf ("  MB/sec:     %.2f\n", mb_rate);
    printf ("  latency:    %.2f us/msg (round-trip)\n",
            (double) (t_end - t_start) * 1000.0 / MSG_COUNT);

    mb_close (s2);
    mb_close (s1);
    return 0;
}
