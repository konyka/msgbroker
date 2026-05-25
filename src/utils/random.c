#include "random.h"

#include <string.h>

#if defined _WIN32
#include "win.h"
#include <wincrypt.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#endif

static uint64_t mb_random_state [2];

static uint64_t mb_random_splitmix64 (uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void mb_random_seed (void)
{
#if defined _WIN32
    HCRYPTPROV prov;
    if (CryptAcquireContext (&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom (prov, sizeof (mb_random_state), (BYTE *) mb_random_state);
        CryptReleaseContext (prov, 0);
        return;
    }
#else
    int fd = open ("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read (fd, mb_random_state, sizeof (mb_random_state));
        close (fd);
        return;
    }
#endif
    uint64_t seed = (uint64_t) time (NULL) ^ ((uint64_t) clock () << 32);
    mb_random_state [0] = mb_random_splitmix64 (&seed);
    mb_random_state [1] = mb_random_splitmix64 (&seed);
}

static inline uint64_t mb_random_xoroshiro128plus (void)
{
    uint64_t s0 = mb_random_state [0];
    uint64_t s1 = mb_random_state [1];
    uint64_t result = s0 + s1;
    s1 ^= s0;
    mb_random_state [0] = ((s0 << 55) | (s0 >> 9)) ^ s1;
    mb_random_state [1] = ((s1 << 36) | (s1 >> 28));
    return result;
}

uint64_t mb_random_generate (void)
{
    return mb_random_xoroshiro128plus ();
}

void mb_random_fill (void *buf, size_t len)
{
    uint8_t *p = (uint8_t *) buf;
    while (len >= 8) {
        uint64_t val = mb_random_xoroshiro128plus ();
        memcpy (p, &val, 8);
        p += 8;
        len -= 8;
    }
    if (len > 0) {
        uint64_t val = mb_random_xoroshiro128plus ();
        memcpy (p, &val, len);
    }
}
