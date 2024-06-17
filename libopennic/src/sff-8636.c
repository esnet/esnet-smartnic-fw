#include "sff-8636.h"
#include "sff-8636-types.h"

//--------------------------------------------------------------------------------------------------
const char* sff_8636_compliance_revision_to_str(enum sff_8636_compliance_revision compliance) {
    const char* str = "UNKNOWN";
    switch (compliance) {
#define CASE_COMP(_name, _text) case sff_8636_compliance_revision_##_name: str = _text; break
    CASE_COMP(NOT_SPECIFIED, "Revision not specified");
    CASE_COMP(SFF_8436_4_8_OR_EARLIER, "SFF-8436 Rev 4.8 or earlier");
    CASE_COMP(SFF_8436_4_8_OR_EARLIER_WITH_EXCEPTIONS,
              "Includes functionality described in revision 4.8 or earlier of SFF-8436");
    CASE_COMP(SFF_8636_1_3_OR_EARLIER, "SFF-8636 Rev 1.3 or earlier");
    CASE_COMP(SFF_8636_1_4, "SFF-8636 Rev 1.4");
    CASE_COMP(SFF_8636_1_5, "SFF-8636 Rev 1.5");
    CASE_COMP(SFF_8636_2_0, "SFF-8636 Rev 2.0");
    CASE_COMP(SFF_8636_2_5_OR_2_6_OR_2_7, "SFF-8636 Rev 2.5, 2.6 and 2.7");
    CASE_COMP(SFF_8636_2_8_OR_2_9_OR_2_10, "SFF-8636 Rev 2.8, 2.9 and 2.10");

    case sff_8636_compliance_revision_RESERVED_FIRST ... sff_8636_compliance_revision_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_COMP
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8636_tx_technology_to_str(enum sff_8636_tx_technology tech) {
    const char* str = "UNKNOWN";
    switch (tech) {
#define CASE_TECH(_name, _text) case sff_8636_tx_technology_##_name: str = _text; break
    CASE_TECH(850_NM_VCSEL, "850 nm VCSEL");
    CASE_TECH(1310_NM_VCSEL, "1310 nm VCSEL");
    CASE_TECH(1550_NM_VCSEL, "1550 nm VCSEL");
    CASE_TECH(1310_NM_FP, "1310 nm FP");
    CASE_TECH(1310_NM_DFB, "1310 nm DFB");
    CASE_TECH(1550_NM_DFB, "1550 nm DFB");
    CASE_TECH(1310_NM_EML, "1310 nm EML");
    CASE_TECH(1550_NM_EML, "1550 nm EML");
    CASE_TECH(OTHER, "Other / Undefined");
    CASE_TECH(1490_NM_DFB, "1490 nm DFB");
    CASE_TECH(COPPER_UNEQ, "Copper cable unequalized");
    CASE_TECH(COPPER_PASSIVE_EQ, "Copper cable passive equalized");
    CASE_TECH(COPPER_NE_FE_ACTIVE_EQ, "Copper cable, near and far end limiting active equalizers");
    CASE_TECH(COPPER_FE_ACTIVE_EQ, "Copper cable, far end limiting active equalizers");
    CASE_TECH(COPPER_NE_ACTIVE_EQ, "Copper cable, near end limiting active equalizers");
    CASE_TECH(COPPER_LINEAR_ACTIVE_EQ, "Copper cable, linear active equalizers");
#undef CASE_TECH
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8636_param_type_to_str(enum sff_8636_param_type type) {
    const char* str = "UNKNOWN";
    switch (type) {
#define CASE_TYPE(_name, _text) case sff_8636_param_type_##_name: str = _text; break
    CASE_TYPE(NOT_SUPPORTED, "Parameter not supported");
    CASE_TYPE(SNR, "SNR, line ingress");
    CASE_TYPE(RESIDUAL_ISI_DISPERSION, "Residual ISI/Dispersion, line ingress");
    CASE_TYPE(PAM4_LEVEL_TRANSITION, "PAM4 Level Transition Parameter, line ingress");
    CASE_TYPE(PRE_FEC_BER_AVERAGE, "Pre-FEC BER, average, line ingress");
    CASE_TYPE(FEC_AVERAGE, "FER, average, line ingress");
    CASE_TYPE(TEC_CURRENT, "TEC Current");
    CASE_TYPE(LASER_FREQUENCY, "Laser Frequency");
    CASE_TYPE(LASER_TEMPERATURE, "Laser Temperature");
    CASE_TYPE(PRE_FEC_BER_LATCHED_MIN,
              "Pre-FEC BER, latched minimum value since last read, line ingress");
    CASE_TYPE(PRE_FEC_BER_LATCHED_MAX,
              "Pre-FEC BER, latched maximum value since last read, line ingress");
    CASE_TYPE(PRE_FEC_BER_PRIOR_PERIOD, "Pre-FEC BER, prior period, line ingress");
    CASE_TYPE(PRE_FEC_BER_CURRENT, "Pre-FEC BER, current, line ingress");
    CASE_TYPE(FER_LATCHED_MIN, "FER, latched minimum value since last read, line ingress");
    CASE_TYPE(FER_LATCHED_MAX, "FER, latched maximum value since last read, line ingress");
    CASE_TYPE(FER_PRIOR_PERIOD, "FER, prior period, line ingress");
    CASE_TYPE(FER_CURRENT, "FER, current, line ingress");

    case sff_8636_param_type_RESERVED_FIRST ... sff_8636_param_type_RESERVED_LAST:
        str = "RESERVED";
        break;

    case sff_8636_param_type_VENDOR_FIRST ... sff_8636_param_type_VENDOR_LAST:
        str = "VENDOR";
        break;
#undef CASE_TYPE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8636_lane_mapping_to_str(enum sff_8636_lane_mapping mapping) {
    const char* str = "UNKNOWN";
    switch (mapping) {
#define CASE_MAP(_name, _text) case sff_8636_lane_mapping_##_name: str = _text; break
    CASE_MAP(NOT_SUPPORTED, "Not determined or not supported");
    CASE_MAP(OPTICAL_LANE_1_LSB, "Optical Lane 1, LSB");
    CASE_MAP(OPTICAL_LANE_1_MSB, "Optical Lane 1, MSB");
    CASE_MAP(OPTICAL_LANE_2_LSB, "Optical Lane 2, LSB");
    CASE_MAP(OPTICAL_LANE_2_MSB, "Optical Lane 2, MSB");
    CASE_MAP(OPTICAL_LANE_1, "Optical Lane 1");
    CASE_MAP(OPTICAL_LANE_2, "Optical Lane 2");

    case sff_8636_lane_mapping_RESERVED_FIRST ... sff_8636_lane_mapping_RESERVED_LAST:
        str = "RESERVED";
        break;

    case sff_8636_lane_mapping_VENDOR_FIRST ... sff_8636_lane_mapping_VENDOR_LAST:
        str = "VENDOR";
        break;
#undef CASE_MAP
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8636_state_duration_to_str(enum sff_8636_state_duration duration) {
    const char* str = "UNKNOWN";
    switch (duration) {
#define CASE_DURATION(_name, _text) case sff_8636_state_duration_##_name: str = _text; break
    CASE_DURATION(LESS_THAN_1_MS, "Maximum state duration is less than 1 ms");
    CASE_DURATION(1_MS_TO_5_MS, "1 ms <= maximum state duration < 5 ms");
    CASE_DURATION(5_MS_TO_10_MS, "5 ms <= maximum state duration < 10 ms");
    CASE_DURATION(10_MS_TO_50_MS, "10 ms <= maximum state duration < 50 ms");
    CASE_DURATION(50_MS_TO_100_MS, "50 ms <= maximum state duration < 100 ms");
    CASE_DURATION(100_MS_TO_500_MS, "100 ms <= maximum state duration < 500 ms");
    CASE_DURATION(500_MS_TO_1_S, "500 ms <= maximum state duration < 1 s");
    CASE_DURATION(1_S_TO_5_S, "1 s <= maximum state duration < 5 s");
    CASE_DURATION(5_S_TO_10_S, "5 s <= maximum state duration < 10 s");
    CASE_DURATION(10_S_TO_1_MIN, "10 s <= maximum state duration < 1 min");
    CASE_DURATION(1_MIN_TO_5_MIN, "1 min <= maximum state duration < 5 min");
    CASE_DURATION(5_MIN_TO_10_MIN, "5 min <= maximum state duration < 10 min");
    CASE_DURATION(10_MIN_TO_50_MIN, "10 min <= maximum state duration < 50 min");
    CASE_DURATION(GREATER_THAN_50_MIN, "Maximum state duration >= 50 min");

    case sff_8636_state_duration_RESERVED_FIRST ... sff_8636_state_duration_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_DURATION
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8636_module_type_to_str(enum sff_8636_module_type type) {
    const char* str = "UNKNOWN";
    switch (type) {
#define CASE_TYPE(_name, _text) case sff_8636_module_type_##_name: str = _text; break
    CASE_TYPE(UNDEFINED, "Undefined");
    CASE_TYPE(OPTICAL_MMF, "Optical Interfaces: MMF");
    CASE_TYPE(OPTICAL_SMF, "Optical Interfaces: SMF");
    CASE_TYPE(PASSIVE_COPPER, "Passive Copper");
    CASE_TYPE(ACTIVE_CABLE, "Active Cable");
    CASE_TYPE(BASE_T, "Base-T");

    case sff_8636_module_type_CUSTOM_FIRST ... sff_8636_module_type_CUSTOM_LAST:
        str = "CUSTOM";
        break;

    case sff_8636_module_type_RESERVED0_FIRST ... sff_8636_module_type_RESERVED0_LAST:
    case sff_8636_module_type_RESERVED1_FIRST ... sff_8636_module_type_RESERVED1_LAST:
        str = "RESERVED";
        break;
#undef CASE_TYPE
    }

    return str;
}
