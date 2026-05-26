#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    int fd[2];

    if (size < 2 || size > 131072)
        return 0;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
        return 0;

    write (fd[1], data, size);
    close (fd[1]);

    {
        uint8_t hdr[2];
        ssize_t nr = read (fd[0], hdr, 2);
        if (nr < 2) { close (fd[0]); return 0; }

        {
            uint8_t plen = hdr[1] & 0x7F;
            int payload_len;

            if (plen == 126) {
                uint8_t ext[2];
                if (read (fd[0], ext, 2) < 2) { close (fd[0]); return 0; }
                payload_len = (ext[0] << 8) | ext[1];
            } else if (plen == 127) {
                uint8_t ext[8];
                if (read (fd[0], ext, 8) < 8) { close (fd[0]); return 0; }
                payload_len = (int)((ext[4] << 24) | (ext[5] << 16) |
                    (ext[6] << 8) | ext[7]);
            } else {
                payload_len = plen;
            }

            if (payload_len > 0 && payload_len <= 131072) {
                uint8_t *body = (uint8_t *) malloc ((size_t) payload_len);
                if (body) {
                    size_t pos = 0;
                    while (pos < (size_t) payload_len) {
                        nr = read (fd[0], body + pos,
                            (size_t) payload_len - pos);
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
    uint8_t buf[131072];
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
