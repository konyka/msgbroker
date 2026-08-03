// SIPC protocol fuzzer. Input contract:
//   bytes [0..4)  -> 4-byte big-endian declared body length (parser rejects
//                    if zero or > 65536).
//   bytes [4..N)  -> body bytes (truncation / short-read are exercised).
// The harness writes the header + body to one end of a socketpair and walks
// the parser on the other end.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>

#include <fuzzer/FuzzedDataProvider.h>

extern "C" {
#include "../../src/transport/ipc/sipc.h"
#include "../../src/core/ep.h"
#include "../../src/core/sock.h"
#include "../../src/memory/msg.h"
}

namespace {

constexpr uint32_t kMaxFrame = 65536;

void consume_body (int fd, uint32_t body_sz) {
    if (body_sz == 0)
        return;
    uint8_t *body = (uint8_t *) std::malloc (body_sz);
    if (!body)
        return;
    size_t pos = 0;
    while (pos < body_sz) {
        ssize_t nr = ::read (fd, body + pos, body_sz - pos);
        if (nr <= 0)
            break;
        pos += (size_t) nr;
    }
    std::free (body);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size) {
    if (size < 4)
        return 0;

    FuzzedDataProvider fdp (data, size);

    uint32_t hdr_raw = fdp.ConsumeIntegral<uint32_t> ();
    uint32_t declared_body = hdr_raw;
    if (declared_body > kMaxFrame)
        declared_body = kMaxFrame;

    std::vector<uint8_t> body = fdp.ConsumeRemainingBytes<uint8_t> ();

    int fd[2];
    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
        return 0;

    uint8_t hdr_be[4] = {
        (uint8_t) (hdr_raw >> 24), (uint8_t) (hdr_raw >> 16),
        (uint8_t) (hdr_raw >> 8),  (uint8_t)  hdr_raw
    };
    if (::write (fd[1], hdr_be, 4) != 4) {
        ::close (fd[0]); ::close (fd[1]);
        return 0;
    }
    if (!body.empty ())
        (void) ::write (fd[1], body.data (), body.size ());
    ::close (fd[1]);

    uint8_t in_hdr[4];
    ssize_t nr = ::read (fd[0], in_hdr, 4);
    if (nr < 4) { ::close (fd[0]); return 0; }

    uint32_t in_body_sz = ((uint32_t) in_hdr[0] << 24) |
                          ((uint32_t) in_hdr[1] << 16) |
                          ((uint32_t) in_hdr[2] << 8)  |
                           (uint32_t) in_hdr[3];
    if (in_body_sz > 0 && in_body_sz <= kMaxFrame)
        consume_body (fd[0], in_body_sz);

    ::close (fd[0]);
    return 0;
}