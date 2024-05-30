#include "sysmon.h"

#include "array_size.h"
#include "memory-barriers.h"
#include "stats.h"
#include <stdint.h>
#include <unistd.h>
#include "unused.h"

// Temporary structures until regmap YAML changes pushed into open-nic-shell.
#include "sysmon_2_block.h"

//--------------------------------------------------------------------------------------------------
/*
 * Reference: https://www.xilinx.com/support/documentation/user_guides/ug580-ultrascale-sysmon.pdf
 */

/*
 * Power Supply Sensor (Page 41)
 *
 * Convert raw 10-bit ADC value to voltage
 */
static float adc_to_v(uint16_t adc_code) {
  return(((float)adc_code) / 1024.0f);
}

/*
 * Temperature Sensor (Page 40)
 *
 * The SYSMON contains a temperature sensor that produces a voltage output
 * proportional to the die temperature.
 *
 * Ultrascale+ FPGAs have the SYSMONE4 block
 *
 */

static float v_to_degc_sysmone4(float v, enum sysmon_flag_ref ref) {
  if (ref == SYSMON_FLAG_REF_INTERNAL) {
    return (v * 509.3140064 - 280.23087870);
  } else {
    return (v * 507.5921310 - 279.42657680);
  }
}

float sysmon_get_temp(volatile struct sysmon_block * _sysmon) {
  volatile struct sysmon_2_block* sysmon = (typeof(sysmon))_sysmon;
  float v = adc_to_v(sysmon->temperature.adc);

  return (v_to_degc_sysmone4(v, sysmon->flag.ref));
}

//--------------------------------------------------------------------------------------------------
void sysmon_master_reset(volatile struct sysmon_block* _sysmon) {
    volatile struct sysmon_2_block* sysmon = (typeof(sysmon))_sysmon;

    sysmon->software_reset = 0xa;
    barrier();
    usleep(1 * 1000);
}

//--------------------------------------------------------------------------------------------------
void sysmon_sequencer_enable(volatile struct sysmon_block* _sysmon,
                             uint64_t select_mask,
                             uint64_t average_mask) {
    volatile struct sysmon_2_block* sysmon = (typeof(sysmon))_sysmon;

    // Disable the sequencer if running.
    union sysmon_2_config_reg_1 config_reg_1 = {._v = sysmon->config_reg_1._v};
    config_reg_1.seq = SYSMON_2_CONFIG_REG_1_SEQ_SINGLE_CHANNEL;
    sysmon->config_reg_1._v = config_reg_1._v;
    barrier();

    // Set the selected channels for the sequencer to convert.
    sysmon->seqchsel0._v = select_mask & 0xffff;
    sysmon->seqavg0._v = average_mask & 0xffff;
    sysmon->seqchsel1._v = (select_mask >> 16) & 0xffff;
    sysmon->seqavg1._v = (average_mask >> 16) & 0xffff;
    sysmon->seqchsel2._v = (select_mask >> 32) & 0xffff;
    sysmon->seqavg2._v = (average_mask >> 32) & 0xffff;
    barrier();

    // Enable the sequencer.
    config_reg_1.seq = SYSMON_2_CONFIG_REG_1_SEQ_CONTINUOUS;
    sysmon->config_reg_1._v = config_reg_1._v;
    barrier();
}

//--------------------------------------------------------------------------------------------------
struct sysmon_metric {
    struct stats_metric_spec spec;
    uint64_t channel_mask;
};

#define SYSMON_METRIC(_name, _channel_mask) \
{ \
    .spec = STATS_METRIC_SPEC_IO( \
        #_name, NULL, GAUGE, 0, 0, \
        struct sysmon_2_block, _name, 10, 6, false, STATS_IO_DATA_NULL \
    ), \
    .channel_mask = _channel_mask, \
}

#define SYSMON_METRIC_MIN_MAX(_name, _channel_mask) \
    SYSMON_METRIC(_name, _channel_mask), \
    SYSMON_METRIC(min_##_name, _channel_mask), \
    SYSMON_METRIC(max_##_name, _channel_mask)

#define SYSMON_METRIC_ALARM(_name, _pos, _channel_mask) \
{ \
    .spec = STATS_METRIC_SPEC_IO( \
        #_name "_alarm", NULL, FLAG, 0, 0, \
        struct sysmon_2_block, alarm_output_status, 1, _pos, false, STATS_IO_DATA_NULL \
     ), \
    .channel_mask = _channel_mask, \
}

static const struct sysmon_metric sysmon_metrics[] = {
#define CM(_channel) SYSMON_CHANNEL_MASK(_channel)

    SYSMON_METRIC_MIN_MAX(temperature, CM(TEMP)),
    SYSMON_METRIC_MIN_MAX(vccint,      CM(VCCINT)),
    SYSMON_METRIC_MIN_MAX(vccaux,      CM(VCCAUX)),
    SYSMON_METRIC_MIN_MAX(vccbram,     CM(VCCBRAM)),
    SYSMON_METRIC_MIN_MAX(vcc_psintlp, CM(VCC_PSINTLP)),
    SYSMON_METRIC_MIN_MAX(vcc_psintfp, CM(VCC_PSINTFP)),
    SYSMON_METRIC_MIN_MAX(vcc_psaux,   CM(VCC_PSAUX)),
    SYSMON_METRIC_MIN_MAX(vuser0,      CM(VUSER0)),
    SYSMON_METRIC_MIN_MAX(vuser1,      CM(VUSER1)),
    SYSMON_METRIC_MIN_MAX(vuser2,      CM(VUSER2)),
    SYSMON_METRIC_MIN_MAX(vuser3,      CM(VUSER3)),

    SYSMON_METRIC(vp_vn,               CM(VP_VN)),
    SYSMON_METRIC(vrefp,               CM(VREFP)),
    SYSMON_METRIC(vrefn,               CM(VREFN)),

    SYSMON_METRIC(vaux[0],             CM(VAUX0)),
    SYSMON_METRIC(vaux[1],             CM(VAUX1)),
    SYSMON_METRIC(vaux[2],             CM(VAUX2)),
    SYSMON_METRIC(vaux[3],             CM(VAUX3)),
    SYSMON_METRIC(vaux[4],             CM(VAUX4)),
    SYSMON_METRIC(vaux[5],             CM(VAUX5)),
    SYSMON_METRIC(vaux[6],             CM(VAUX6)),
    SYSMON_METRIC(vaux[7],             CM(VAUX7)),
    SYSMON_METRIC(vaux[8],             CM(VAUX8)),
    SYSMON_METRIC(vaux[9],             CM(VAUX9)),
    SYSMON_METRIC(vaux[10],            CM(VAUX10)),
    SYSMON_METRIC(vaux[11],            CM(VAUX11)),
    SYSMON_METRIC(vaux[12],            CM(VAUX12)),
    SYSMON_METRIC(vaux[13],            CM(VAUX13)),
    SYSMON_METRIC(vaux[14],            CM(VAUX14)),
    SYSMON_METRIC(vaux[15],            CM(VAUX15)),

    SYSMON_METRIC_ALARM(over_temperature, 0,  CM(TEMP)),
    SYSMON_METRIC_ALARM(temperature,      1,  CM(TEMP)),
    SYSMON_METRIC_ALARM(vccint,           2,  CM(VCCINT)),
    SYSMON_METRIC_ALARM(vccaux,           3,  CM(VCCAUX)),
    SYSMON_METRIC_ALARM(vccbram,          4,  CM(VCCBRAM)),
    SYSMON_METRIC_ALARM(any,              8,  CM(TEMP)   | CM(VCCINT) | CM(VCCAUX) | CM(VCCBRAM) | \
                                              CM(VUSER0) | CM(VUSER1) | CM(VUSER2) | CM(VUSER3)),
    SYSMON_METRIC_ALARM(vuser0,           9,  CM(VUSER0)),
    SYSMON_METRIC_ALARM(vuser1,           10, CM(VUSER1)),
    SYSMON_METRIC_ALARM(vuser2,           11, CM(VUSER2)),
    SYSMON_METRIC_ALARM(vuser3,           12, CM(VUSER3)),
    SYSMON_METRIC_ALARM(any_vuser,        16, CM(VUSER0) | CM(VUSER1) | CM(VUSER2) | CM(VUSER3)),

#undef CM
};

//--------------------------------------------------------------------------------------------------
union sysmon_metric_flags {
    struct {
        uint64_t external : 1; // [0]
        uint64_t bipolar  : 1; // [1]
        uint64_t temp     : 1; // [2]
        uint64_t supply   : 1; // [3]
    };
    uint64_t _v;
} __attribute__((packed));

static double sysmon_stats_convert_metric(const struct stats_block_spec* UNUSED(bspec),
                                          const struct stats_metric_spec* mspec,
                                          uint64_t value,
                                          void* UNUSED(data)) {
    union sysmon_metric_flags flags = {._v = mspec->io.data.u64};

    // Only convert the monitor metrics.
    if (mspec->type != stats_metric_type_GAUGE) {
        return value;
    }

    // Convert the 10 bit A/D measurement to a voltage.
    double voltage;
    if (flags.bipolar && value >= (1 << 9)) {
        // Value is negative in 2's complement format.
        voltage = -((double)((~value & ((1 << 10) - 1)) + 1));
    } else {
        voltage = (double)value;
    }
    voltage /= 1024.0; // Scale according to the A/D transfer transfer function at 10 bits.

    double slope = 1.0;
    double offset = 0.0;
    if (flags.temp) {
        if (flags.external) {
            slope = 507.5921310;
            offset = -279.42657680;
        } else {
            slope = 509.3140064;
            offset = -280.23087870;
        }
    } else if (flags.supply) {
        slope = 3.0;
    }

    return voltage * slope + offset;
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* sysmon_stats_zone_alloc(struct stats_domain* domain,
                                           volatile struct sysmon_block* _sysmon,
                                           const char* name) {
    volatile struct sysmon_2_block* sysmon = (typeof(sysmon))_sysmon;
    bool external = sysmon->flag.ref == SYSMON_2_FLAG_REF_EXTERNAL;

    uint64_t select_mask = sysmon->seqchsel2._v & 0xffff;
    select_mask <<= 16;
    select_mask |= sysmon->seqchsel1._v & 0xffff;
    select_mask <<= 16;
    select_mask |= sysmon->seqchsel0._v & 0xffff;

    uint64_t bipolar_mask = sysmon->seqinmode1._v & 0xffff;
    bipolar_mask <<= 16;
    bipolar_mask |= sysmon->seqinmode0._v & 0xffff;
    bipolar_mask <<= 16;

    unsigned int nmetrics = 0;
    const struct sysmon_metric* sm;
    for (sm = sysmon_metrics; sm < &sysmon_metrics[ARRAY_SIZE(sysmon_metrics)]; ++sm) {
        if ((sm->channel_mask & select_mask) != 0) {
            nmetrics += 1;
        }
    }

    struct stats_metric_spec mspecs[nmetrics];
    struct stats_metric_spec* mspec = mspecs;
    for (sm = sysmon_metrics; sm < &sysmon_metrics[ARRAY_SIZE(sysmon_metrics)]; ++sm) {
        if ((sm->channel_mask & select_mask) == 0) {
            continue;
        }

        union sysmon_metric_flags flags = {
            .external = external ? 1 : 0,
            .bipolar = (sm->channel_mask & bipolar_mask & SYSMON_ANALOG_CHANNELS_MASK) != 0 ? 1 : 0,
            .temp = (sm->channel_mask & SYSMON_TEMP_CHANNELS_MASK) != 0 ? 1 : 0,
            .supply = (sm->channel_mask & SYSMON_SUPPLY_CHANNELS_MASK) != 0 ? 1 : 0,
        };

        *mspec = sm->spec;
        mspec->io.data.u64 = flags._v;
        mspec += 1;
    }

    struct stats_block_spec bspecs[] = {
        {
            .name = "sysmon",
            .metrics = mspecs,
            .nmetrics = nmetrics,
            .io = {
                .base = sysmon,
            },
            .convert_metric = sysmon_stats_convert_metric,
        },
    };
    struct stats_zone_spec zspec = {
        .name = name,
        .blocks = bspecs,
        .nblocks = ARRAY_SIZE(bspecs),
    };
    return stats_zone_alloc(domain, &zspec);
}

//--------------------------------------------------------------------------------------------------
void sysmon_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}
