/*
    inproc_lat.c — In-process one-way latency benchmark (PUSH/PULL)
    Measures per-message one-way latency in microseconds.
*/
#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define MSG_SIZE    64
#define MSG_COUNT   100000

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
    double elapsed, lat_us;

    memset (send_buf, 'B', MSG_SIZE);

    s1 = mb_socket (AF_MB, MB_PAIR);
    s2 = mb_socket (AF_MB, MB_PAIR);
    if (s1 < 0 || s2 < 0) {
        fprintf (stderr, "socket creation failed (err=%d)\n", mb_errno ());
        return 1;
    }

    mb_bind (s1, "inproc://lat");
    mb_connect (s2, "inproc://lat");

    printf ("inproc latency: %d messages of %d bytes (round-trip PAIR)\n",
            MSG_COUNT, MSG_SIZE);

    for (i = 0; i < 100; i++) {
        mb_send (s2, send_buf, MSG_SIZE, 0);
        mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    }

    t_start = now_ms ();
    for (i = 0; i < MSG_COUNT; i++) {
        mb_send (s2, send_buf, MSG_SIZE, 0);
        mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    }
    t_end = now_ms ();

    elapsed = (double) (t_end - t_start) / 1000.0;
    if (elapsed <= 0.0)
        elapsed = 0.001;
    lat_us = elapsed * 1e6 / (double) MSG_COUNT / 2.0;

    printf ("  elapsed:    %.3f s\n", elapsed);
    printf ("  latency:    %.2f us/msg (one-way, half round-trip)\n", lat_us);
    printf ("  msg/sec:    %.0f (round-trip)\n", (double) MSG_COUNT / elapsed);

    mb_close (s1);
    mb_close (s2);
    return 0;
}
