#include "qdma.h"

#include "array_size.h"
#include "memory-barriers.h"
#include "qdma_function_block.h"
#include <stdbool.h>
#include <stddef.h>

//--------------------------------------------------------------------------------------------------
bool qdma_get_queues(volatile struct qdma_function_block* qdma,
                     unsigned int* base_queue, unsigned int* num_queues) {
    union qdma_function_qconf qconf = {._v = qdma->qconf._v};

    *base_queue = qconf.qbase;
    *num_queues = qconf.numq;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool qdma_set_queues(volatile struct qdma_function_block* qdma,
                     unsigned int base_queue, unsigned int num_queues) {
#define FUNCTION_QUEUES ARRAY_SIZE(qdma->indir_table)
#define MAX_QUEUES (2 * FUNCTION_QUEUES) // TODO: what is the actual total?

    if (num_queues > FUNCTION_QUEUES || base_queue + num_queues > MAX_QUEUES) {
        return false;
    }

    unsigned int spread = num_queues == 0 ? 1 : num_queues;
    for (unsigned int q = 0; q < FUNCTION_QUEUES; ++q) {
        qdma->indir_table[q] = q % spread;
    }
    barrier();

    union qdma_function_qconf qconf = {
        .qbase = base_queue,
        .numq = num_queues,
    };
    qdma->qconf._v = qconf._v;

    return true;

#undef MAX_QUEUES
#undef FUNCTION_QUEUES
}
