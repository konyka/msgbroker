// msgqueue (inproc) fuzzer. Input contract:
//   bytes [0..N)  -> opaque payload bytes fed through mb_msgqueue_push in a
//                    tight loop bounded by both `size` and 256 iterations.
// FuzzedDataProvider is used to derive structured parameters (per-iteration
// payload length, per-iteration offset) from the raw bytes so that mutations
// can target the boundaries that matter (length=0, length=63, length=64, etc.).

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" {
#include "../../src/transport/inproc/msgqueue.h"
#include "../../src/memory/msg.h"
#include "../../src/utils/alloc.h"
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size) {
    if (size < 1)
        return 0;

    FuzzedDataProvider fdp (data, size);

    struct mb_msgqueue mq;
    mb_msgqueue_init (&mq, 0);

    // Cap iterations at 256 to mirror the original harness while letting
    // FDP drive per-step payload length and source offset.
    std::vector<uint8_t> scratch = fdp.ConsumeRemainingBytes<uint8_t> ();
    size_t scratch_len = scratch.size ();

    for (size_t i = 0; i < scratch_len && i < 256; i++) {
        // FDP gives us a deterministic per-iteration length in [0, 64); the
        // %64 in the original is preserved as a wrap into the same range.
        size_t take = fdp.ConsumeIntegralInRange<size_t> (0, 63);
        size_t off = i % (scratch_len > 0 ? scratch_len : 1);
        if (off + take > scratch_len)
            take = (scratch_len > off) ? scratch_len - off : 0;

        struct mb_msg in_msg;
        mb_msg_init_data (&in_msg, scratch.data () + off, take);
        if (mb_msgqueue_push (&mq, &in_msg) < 0)
            break;
    }

    struct mb_msg out_msg;
    while (!mb_msgqueue_empty (&mq)) {
        mb_msgqueue_pop (&mq, &out_msg);
        mb_msg_term (&out_msg);
    }

    mb_msgqueue_term (&mq);
    return 0;
}