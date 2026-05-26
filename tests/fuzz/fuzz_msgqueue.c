#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../src/transport/inproc/msgqueue.h"
#include "../../src/memory/msg.h"
#include "../../src/utils/alloc.h"

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    struct mb_msgqueue mq;
    struct mb_msg in_msg;
    struct mb_msg out_msg;
    size_t i;

    if (size < 1 || size > 4096)
        return 0;

    mb_msgqueue_init (&mq, 0);

    for (i = 0; i < size && i < 256; i++) {
        mb_msg_init_data (&in_msg, data + (i % (size - i > 0 ? size - i : 1)),
            (size_t) (data[i] % 64));
        if (mb_msgqueue_push (&mq, &in_msg) < 0)
            break;
    }

    while (!mb_msgqueue_empty (&mq)) {
        mb_msgqueue_pop (&mq, &out_msg);
        mb_msg_term (&out_msg);
    }

    mb_msgqueue_term (&mq);
    return 0;
}

int main (int argc, char **argv)
{
    uint8_t buf[4096];
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
