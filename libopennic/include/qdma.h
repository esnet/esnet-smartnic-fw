#ifndef INCLUDE_QDMA_H
#define INCLUDE_QDMA_H

#include "qdma_function_block.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
bool qdma_get_queues(volatile struct qdma_function_block* qdma,
                     unsigned int* base_queue, unsigned int* num_queues);
bool qdma_set_queues(volatile struct qdma_function_block* qdma,
                     unsigned int base_queue, unsigned int num_queues);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_QDMA_H
