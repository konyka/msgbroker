#include "../../src/utils/queue.h"

#include <assert.h>

int main (void)
{
    struct mb_mpsc_queue queue;

    mb_mpsc_queue_init (&queue);
    mb_mpsc_queue_term (&queue);
    mb_mpsc_queue_term (&queue);

    return 0;
}
