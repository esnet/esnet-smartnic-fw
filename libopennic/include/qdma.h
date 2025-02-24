#ifndef INCLUDE_QDMA_H
#define INCLUDE_QDMA_H

#include "smartnic.h"
#include "stats.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
bool qdma_channel_get_queues(volatile struct esnet_smartnic_bar2* bar2, unsigned int channel,
                             unsigned int* base_queue, unsigned int* num_queues);
bool qdma_channel_set_queues(volatile struct esnet_smartnic_bar2* bar2, unsigned int channel,
                             unsigned int base_queue, unsigned int num_queues);

#define QDMA_CHANNEL_MAX 2
enum qdma_function {
    qdma_function_MIN,
    qdma_function_PF = qdma_function_MIN,

    qdma_function_VF_MIN,
    qdma_function_VF0 = qdma_function_VF_MIN,
    qdma_function_VF1,
    qdma_function_VF2,
    qdma_function_VF_MAX,

    qdma_function_MAX = qdma_function_VF_MAX
};
bool qdma_function_get_queues(volatile struct esnet_smartnic_bar2* bar2,
                              unsigned int channel, enum qdma_function func,
                              unsigned int* base_queue, unsigned int* num_queues);
bool qdma_function_set_queues(volatile struct esnet_smartnic_bar2* bar2,
                              unsigned int channel, enum qdma_function func,
                              unsigned int base_queue, unsigned int num_queues);

struct stats_zone* qdma_stats_zone_alloc(struct stats_domain* domain,
                                         volatile struct cmac_adapter_block* adapter,
                                         const char* name);
void qdma_stats_zone_free(struct stats_zone* zone);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_QDMA_H
