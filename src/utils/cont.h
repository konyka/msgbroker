#ifndef MB_CONT_H_INCLUDED
#define MB_CONT_H_INCLUDED

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *) 0)->member)
#endif

#define mb_cont(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

#endif
