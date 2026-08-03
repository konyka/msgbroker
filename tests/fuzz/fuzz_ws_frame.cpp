// WebSocket frame fuzzer. Input contract:
//   bytes [0..2)  -> first two frame header bytes (FIN/RSV/opcode and
//                    MASK/payload_len[7]).
//   bytes [2..N)  -> extension bytes (length-ext and masking key), then
//                    payload bytes (truncation / short-read exercised).
// The harness writes everything to one end of a socketpair and walks the
// parser on the other end.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>

#include <fuzzer/FuzzedDataProvider.h>

namespace {

constexpr uint32_t kMaxPayload = 131072;

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
    if (size < 2)
        return 0;

    FuzzedDataProvider fdp (data, size);

    uint8_t b0 = fdp.ConsumeIntegral<uint8_t> ();
    uint8_t b1 = fdp.ConsumeIntegral<uint8_t> ();

    uint32_t declared_payload;
    size_t header_bytes = 2;
    uint8_t plen = b1 & 0x7F;
    if (plen == 126) {
        uint16_t ext = fdp.ConsumeIntegral<uint16_t> ();
        declared_payload = ext;
        header_bytes += 2;
    } else if (plen == 127) {
        uint64_t ext = fdp.ConsumeIntegral<uint64_t> ();
        // Parser only consumes the low 32 bits.
        declared_payload = (uint32_t) ext;
        header_bytes += 8;
    } else {
        declared_payload = plen;
    }

    if (b1 & 0x80)
        header_bytes += 4; // masking key, even if absent in payload stream

    if (declared_payload > kMaxPayload)
        declared_payload = kMaxPayload;

    std::vector<uint8_t> rest = fdp.ConsumeRemainingBytes<uint8_t> ();

    int fd[2];
    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
        return 0;

    if (::write (fd[1], &b0, 1) != 1 ||
        ::write (fd[1], &b1, 1) != 1) {
        ::close (fd[0]); ::close (fd[1]);
        return 0;
    }
    if (!rest.empty ())
        (void) ::write (fd[1], rest.data (), rest.size ());
    ::close (fd[1]);

    uint8_t in_hdr[2];
    ssize_t nr = ::read (fd[0], in_hdr, 2);
    if (nr < 2) { ::close (fd[0]); return 0; }

    uint8_t in_plen = in_hdr[1] & 0x7F;
    uint32_t in_payload_len;
    if (in_plen == 126) {
        uint8_t ext[2];
        if (::read (fd[0], ext, 2) < 2) { ::close (fd[0]); return 0; }
        in_payload_len = ((uint32_t) ext[0] << 8) | ext[1];
    } else if (in_plen == 127) {
        uint8_t ext[8];
        if (::read (fd[0], ext, 8) < 8) { ::close (fd[0]); return 0; }
        in_payload_len = (uint32_t) ((ext[4] << 24) | (ext[5] << 16) |
                                     (ext[6] << 8)  |  ext[7]);
    } else {
        in_payload_len = in_plen;
    }

    if (in_hdr[1] & 0x80) {
        uint8_t mask[4];
        if (::read (fd[0], mask, 4) < 4) { ::close (fd[0]); return 0; }
    }

    if (in_payload_len > 0 && in_payload_len <= kMaxPayload)
        consume_body (fd[0], in_payload_len);

    ::close (fd[0]);
    (void) header_bytes;
    return 0;
}