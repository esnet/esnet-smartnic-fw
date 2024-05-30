#if !defined(INCLUDE_SYSMON_H)
#define INCLUDE_SYSMON_H 1

#include <assert.h>
#include <stdint.h>
#include "stats.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "sysmon_block.h"

//--------------------------------------------------------------------------------------------------
enum sysmon_channel {
    // SEQCHSEL0
    sysmon_channel_VUSER0,
    sysmon_channel_VUSER1,
    sysmon_channel_VUSER2,
    sysmon_channel_VUSER3,

    // SEQCHSEL1
    sysmon_channel_VCC_PSINTLP = 21,
    sysmon_channel_VCC_PSINTFP,
    sysmon_channel_VCC_PSAUX,
    sysmon_channel_TEMP,
    sysmon_channel_VCCINT,
    sysmon_channel_VCCAUX,
    sysmon_channel_VP_VN,
    sysmon_channel_VREFP,
    sysmon_channel_VREFN,
    sysmon_channel_VCCBRAM,

    // SEQCHSEL2
    sysmon_channel_VAUX_FIRST = 32,
    sysmon_channel_VAUX0 = sysmon_channel_VAUX_FIRST,
    sysmon_channel_VAUX1,
    sysmon_channel_VAUX2,
    sysmon_channel_VAUX3,
    sysmon_channel_VAUX4,
    sysmon_channel_VAUX5,
    sysmon_channel_VAUX6,
    sysmon_channel_VAUX7,
    sysmon_channel_VAUX8,
    sysmon_channel_VAUX9,
    sysmon_channel_VAUX10,
    sysmon_channel_VAUX11,
    sysmon_channel_VAUX12,
    sysmon_channel_VAUX13,
    sysmon_channel_VAUX14,
    sysmon_channel_VAUX15,
    sysmon_channel_VAUX_LAST = sysmon_channel_VAUX15,

    sysmon_channel_MAX
};
static_assert(sysmon_channel_MAX <= 8*sizeof(uint64_t),
              "Number of sysmon channels exceeds uint64_t.");

#define _SYSMON_CHANNEL_MASK(_chan) ((uint64_t)1 << (_chan))
#define SYSMON_CHANNEL_MASK(_name)  _SYSMON_CHANNEL_MASK(sysmon_channel_##_name)

#define SYSMON_TEMP_CHANNELS_MASK \
    SYSMON_CHANNEL_MASK(TEMP)

#define SYSMON_SUPPLY_CHANNELS_MASK \
( \
    SYSMON_CHANNEL_MASK(VUSER0) | \
    SYSMON_CHANNEL_MASK(VUSER1) | \
    SYSMON_CHANNEL_MASK(VUSER2) | \
    SYSMON_CHANNEL_MASK(VUSER3) | \
    SYSMON_CHANNEL_MASK(VCC_PSINTLP) | \
    SYSMON_CHANNEL_MASK(VCC_PSINTFP) | \
    SYSMON_CHANNEL_MASK(VCC_PSAUX) | \
    SYSMON_CHANNEL_MASK(VCCINT) | \
    SYSMON_CHANNEL_MASK(VCCAUX) | \
    SYSMON_CHANNEL_MASK(VCCBRAM) \
)

#define SYSMON_ANALOG_CHANNELS_MASK \
( \
    SYSMON_CHANNEL_MASK(VP_VN) | \
    ( \
        ((SYSMON_CHANNEL_MASK(VAUX_LAST) << 1) - 1) & \
        ~(SYSMON_CHANNEL_MASK(VAUX_FIRST) - 1) \
    ) \
)

//--------------------------------------------------------------------------------------------------
float sysmon_get_temp(volatile struct sysmon_block* sysmon);

void sysmon_master_reset(volatile struct sysmon_block* sysmon);
void sysmon_sequencer_enable(volatile struct sysmon_block* sysmon,
                             uint64_t select_mask, uint64_t average_mask);

struct stats_zone* sysmon_stats_zone_alloc(struct stats_domain* domain,
                                           volatile struct sysmon_block* sysmon,
                                           const char* name);
void sysmon_stats_zone_free(struct stats_zone* zone);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SYSMON_H
