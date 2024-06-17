#ifndef INCLUDE_SFF_8636_TYPES_H
#define INCLUDE_SFF_8636_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFF_8636_PAGE_SIZE   128
#define SFF_8636_MEMORY_SIZE (2 * SFF_8636_PAGE_SIZE)

//--------------------------------------------------------------------------------------------------
union sff_8636_u16 {
    struct {
        uint8_t msb;
        uint8_t lsb;
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_f8 {
    struct {
        uint8_t mantissa : 5; // [4:0]
        uint8_t exponent : 3; // [7:5]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
enum sff_8636_compliance_revision {
    sff_8636_compliance_revision_NOT_SPECIFIED,
    sff_8636_compliance_revision_SFF_8436_4_8_OR_EARLIER,
    sff_8636_compliance_revision_SFF_8436_4_8_OR_EARLIER_WITH_EXCEPTIONS,
    sff_8636_compliance_revision_SFF_8636_1_3_OR_EARLIER,
    sff_8636_compliance_revision_SFF_8636_1_4,
    sff_8636_compliance_revision_SFF_8636_1_5,
    sff_8636_compliance_revision_SFF_8636_2_0,
    sff_8636_compliance_revision_SFF_8636_2_5_OR_2_6_OR_2_7,
    sff_8636_compliance_revision_SFF_8636_2_8_OR_2_9_OR_2_10,

    sff_8636_compliance_revision_RESERVED_FIRST = 0x09,
    sff_8636_compliance_revision_RESERVED_LAST = 0xff
};
const char* sff_8636_compliance_revision_to_str(enum sff_8636_compliance_revision compliance);

//--------------------------------------------------------------------------------------------------
enum sff_8636_tx_technology {
    sff_8636_tx_technology_850_NM_VCSEL,
    sff_8636_tx_technology_1310_NM_VCSEL,
    sff_8636_tx_technology_1550_NM_VCSEL,
    sff_8636_tx_technology_1310_NM_FP,
    sff_8636_tx_technology_1310_NM_DFB,
    sff_8636_tx_technology_1550_NM_DFB,
    sff_8636_tx_technology_1310_NM_EML,
    sff_8636_tx_technology_1550_NM_EML,
    sff_8636_tx_technology_OTHER,
    sff_8636_tx_technology_1490_NM_DFB,
    sff_8636_tx_technology_COPPER_UNEQ,
    sff_8636_tx_technology_COPPER_PASSIVE_EQ,
    sff_8636_tx_technology_COPPER_NE_FE_ACTIVE_EQ,
    sff_8636_tx_technology_COPPER_FE_ACTIVE_EQ,
    sff_8636_tx_technology_COPPER_NE_ACTIVE_EQ,
    sff_8636_tx_technology_COPPER_LINEAR_ACTIVE_EQ,
};
const char* sff_8636_tx_technology_to_str(enum sff_8636_tx_technology tech);

//--------------------------------------------------------------------------------------------------
enum sff_8636_param_type {
    sff_8636_param_type_NOT_SUPPORTED,
    sff_8636_param_type_SNR,
    sff_8636_param_type_RESIDUAL_ISI_DISPERSION,
    sff_8636_param_type_PAM4_LEVEL_TRANSITION,
    sff_8636_param_type_PRE_FEC_BER_AVERAGE,
    sff_8636_param_type_FEC_AVERAGE,
    sff_8636_param_type_TEC_CURRENT,
    sff_8636_param_type_LASER_FREQUENCY,
    sff_8636_param_type_LASER_TEMPERATURE,
    sff_8636_param_type_PRE_FEC_BER_LATCHED_MIN,
    sff_8636_param_type_PRE_FEC_BER_LATCHED_MAX,
    sff_8636_param_type_PRE_FEC_BER_PRIOR_PERIOD,
    sff_8636_param_type_PRE_FEC_BER_CURRENT,
    sff_8636_param_type_FER_LATCHED_MIN,
    sff_8636_param_type_FER_LATCHED_MAX,
    sff_8636_param_type_FER_PRIOR_PERIOD,
    sff_8636_param_type_FER_CURRENT,

    sff_8636_param_type_RESERVED_FIRST = 17,
    sff_8636_param_type_RESERVED_LAST = 191,

    sff_8636_param_type_VENDOR_FIRST = 192,
    sff_8636_param_type_VENDOR_LAST = 255,
};
const char* sff_8636_param_type_to_str(enum sff_8636_param_type type);

//--------------------------------------------------------------------------------------------------
enum sff_8636_lane_mapping {
    sff_8636_lane_mapping_NOT_SUPPORTED,
    sff_8636_lane_mapping_OPTICAL_LANE_1_LSB,
    sff_8636_lane_mapping_OPTICAL_LANE_1_MSB,
    sff_8636_lane_mapping_OPTICAL_LANE_2_LSB,
    sff_8636_lane_mapping_OPTICAL_LANE_2_MSB,
    sff_8636_lane_mapping_OPTICAL_LANE_1,
    sff_8636_lane_mapping_OPTICAL_LANE_2,

    sff_8636_lane_mapping_RESERVED_FIRST = 7,
    sff_8636_lane_mapping_RESERVED_LAST = 12,

    sff_8636_lane_mapping_VENDOR_FIRST = 13,
    sff_8636_lane_mapping_VENDOR_LAST = 15,
};
const char* sff_8636_lane_mapping_to_str(enum sff_8636_lane_mapping mapping);

//--------------------------------------------------------------------------------------------------
enum sff_8636_state_duration {
    sff_8636_state_duration_LESS_THAN_1_MS,
    sff_8636_state_duration_1_MS_TO_5_MS,
    sff_8636_state_duration_5_MS_TO_10_MS,
    sff_8636_state_duration_10_MS_TO_50_MS,
    sff_8636_state_duration_50_MS_TO_100_MS,
    sff_8636_state_duration_100_MS_TO_500_MS,
    sff_8636_state_duration_500_MS_TO_1_S,
    sff_8636_state_duration_1_S_TO_5_S,
    sff_8636_state_duration_5_S_TO_10_S,
    sff_8636_state_duration_10_S_TO_1_MIN,
    sff_8636_state_duration_1_MIN_TO_5_MIN,
    sff_8636_state_duration_5_MIN_TO_10_MIN,
    sff_8636_state_duration_10_MIN_TO_50_MIN,
    sff_8636_state_duration_GREATER_THAN_50_MIN,

    sff_8636_state_duration_RESERVED_FIRST = 0xe,
    sff_8636_state_duration_RESERVED_LAST = 0xf,
};
const char* sff_8636_state_duration_to_str(enum sff_8636_state_duration duration);

//--------------------------------------------------------------------------------------------------
enum sff_8636_module_type {
    sff_8636_module_type_UNDEFINED,
    sff_8636_module_type_OPTICAL_MMF,
    sff_8636_module_type_OPTICAL_SMF,
    sff_8636_module_type_PASSIVE_COPPER,
    sff_8636_module_type_ACTIVE_CABLE,
    sff_8636_module_type_BASE_T,

    sff_8636_module_type_RESERVED0_FIRST = 0x06,
    sff_8636_module_type_RESERVED0_LAST = 0x3f,

    sff_8636_module_type_CUSTOM_FIRST = 0x40,
    sff_8636_module_type_CUSTOM_LAST = 0x8f,

    sff_8636_module_type_RESERVED1_FIRST = 0x90,
    sff_8636_module_type_RESERVED1_LAST = 0xff,
};
const char* sff_8636_module_type_to_str(enum sff_8636_module_type type);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_TYPES_H
