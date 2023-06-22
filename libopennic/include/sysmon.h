#if !defined(INCLUDE_SYSMON_H)
#define INCLUDE_SYSMON_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "sysmon_block.h"

extern float sysmon_get_temp(volatile struct sysmon_block * sysmon);

#ifdef __cplusplus
}
#endif

#endif	// INCLUDE_SYSMON_H
