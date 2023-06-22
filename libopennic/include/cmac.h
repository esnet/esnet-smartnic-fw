#if !defined(INCLUDE_CMAC_H)
#define INCLUDE_CMAC_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "cmac_block.h"
#include <stdbool.h>

extern bool cmac_enable(volatile struct cmac_block * cmac);
extern bool cmac_disable(volatile struct cmac_block * cmac);

#ifdef __cplusplus
}
#endif

#endif	// INCLUDE_CMAC_H
