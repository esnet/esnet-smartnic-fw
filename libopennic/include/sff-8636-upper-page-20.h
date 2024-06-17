#ifndef INCLUDE_SFF_8636_UPPER_PAGE_20_H
#define INCLUDE_SFF_8636_UPPER_PAGE_20_H

#include <assert.h>
#include <stdint.h>

#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_20_param_flags {
    struct {
        uint8_t low_warning_2  : 1; // [0]
        uint8_t high_warning_2 : 1; // [1]
        uint8_t low_alarm_2    : 1; // [2]
        uint8_t high_alarm_2   : 1; // [3]
        uint8_t low_warning_1  : 1; // [4]
        uint8_t high_warning_1 : 1; // [5]
        uint8_t low_alarm_1    : 1; // [6]
        uint8_t high_alarm_1   : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_20_param_config_channel {
    struct {
        uint8_t number       : 2; // [1:0]
        uint8_t specific     : 1; // [2]
        uint8_t reserved     : 1; // [3]
        uint8_t threshold_id : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_20_param_config {
    union sff_8636_upper_page_20_param_config_channel channel;
    uint8_t type; // enum sff_8636_param_type
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_20_lane_mapping {
    struct {
        uint16_t mapping_2 : 4; // [3:0] (enum sff_8636_lane_mapping)
        uint16_t mapping_1 : 4; // [7:4] (enum sff_8636_lane_mapping)
        uint16_t mapping_4 : 4; // [11:8] (enum sff_8636_lane_mapping)
        uint16_t mapping_3 : 4; // [15:12] (enum sff_8636_lane_mapping)
    };
    uint16_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_20_other_config_control {
    struct {
        uint8_t reserved    : 7; // [6:0]
        uint8_t error_reset : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_20_other_config {
    union sff_8636_upper_page_20_other_config_control control; // 250
    uint8_t reserved[5];                                       // 251-255
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_20 {
    union sff_8636_upper_page_20_param_flags param_flags[12];    // 128-139
    union sff_8636_upper_page_20_param_flags param_masks[12];    // 140-151
    union sff_8636_u16 param_values[24];                         // 152-199
    struct sff_8636_upper_page_20_param_config param_config[24]; // 200-247
    union sff_8636_upper_page_20_lane_mapping lane_mapping;      // 248-249
    struct sff_8636_upper_page_20_other_config other_config;     // 250-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_20) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 20 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_20_H
