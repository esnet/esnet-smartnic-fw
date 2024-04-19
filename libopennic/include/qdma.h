#ifndef INCLUDE_QDMA_H
#define INCLUDE_QDMA_H

#include "cmac_adapter_block.h"
#include "qdma_function_block.h"
#include "stats.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
bool qdma_get_queues(volatile struct qdma_function_block* qdma,
                     unsigned int* base_queue, unsigned int* num_queues);
bool qdma_set_queues(volatile struct qdma_function_block* qdma,
                     unsigned int base_queue, unsigned int num_queues);

struct stats_zone* qdma_stats_zone_alloc(struct stats_domain* domain,
                                         volatile struct cmac_adapter_block* adapter,
                                         const char* name);
void qdma_stats_zone_free(struct stats_zone* zone);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_QDMA_H
