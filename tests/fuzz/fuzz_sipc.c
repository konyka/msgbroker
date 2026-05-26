#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>

#include "../../src/transport/ipc/sipc.h"
#include "../../src/core/ep.h"
#include "../../src/core/sock.h"
#include "../../src/memory/msg.h"

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    int fd[2];
    uint8_t hdr[4];

    if (size < 4 || size > 65536)
        return 0;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
        return 0;

    write (fd[1], data, size);
    close (fd[1]);

    {
        ssize_t nr;
        size_t pos = 0;
        nr = read (fd[0], hdr, 4);
        if (nr < 4) { close (fd[0]); return 0; }

        {
            uint32_t body_sz = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                ((uint32_t)hdr[2] << 8) | hdr[3];
            if (body_sz > 0 && body_sz <= 65536) {
                uint8_t *body = (uint8_t *) malloc (body_sz);
                if (body) {
                    pos = 0;
                    while (pos < body_sz) {
                        nr = read (fd[0], body + pos, body_sz - pos);
                        if (nr <= 0) break;
                        pos += (size_t) nr;
                    }
                    free (body);
                }
            }
        }
    }

    close (fd[0]);
    return 0;
}

int main (int argc, char **argv)
{
    uint8_t buf[65536];
    size_t n;
    FILE *f;

    (void) argc;
    if (argc < 2) {
        fprintf (stderr, "usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    f = fopen (argv[1], "rb");
    if (!f) { perror ("fopen"); return 1; }
    n = fread (buf, 1, sizeof (buf), f);
    fclose (f);

    return LLVMFuzzerTestOneInput (buf, n);
}
