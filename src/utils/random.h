#ifndef MB_RANDOM_H_INCLUDED
#define MB_RANDOM_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

void mb_random_seed (void);
uint64_t mb_random_generate (void);
void mb_random_fill (void *buf, size_t len);

#endif
