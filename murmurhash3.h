#ifndef MURMURHASH3_H64
#define MURMURHASH3_H64

#include <stdbool.h>
#include <stdlib.h>


bool qhashmurmur3_128(const void *data, size_t nbytes, void *retbuf);

#endif