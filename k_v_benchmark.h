#ifndef __K_V_BENCHMARK_H
#define __K_V_BENCHMARK_H

#include <stdint.h>

typedef enum {
	BM_NONE,
	BM_PRINT,
	BM_DIRECT_FILE,
	BM_TO_QUEUE,
	BM_TO_LOCK_FREE_QUEUE,
	BM_TO_ZEROMQ,
} bm_type_t;

typedef enum {
    BM_READ_OP,
    BM_WRITE_OP,
} bm_op_type_t;

typedef struct {
    bm_op_type_t type;
    uint64_t	 key_hv;
} bm_op_t;

#endif