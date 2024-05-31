#ifndef INCLUDE_SFF_8636_UPPER_PAGE_03_H
#define INCLUDE_SFF_8636_UPPER_PAGE_03_H

#include <assert.h>
#include <stdint.h>

#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_03_monitor_threshold {
    union sff_8636_u16 high;
    union sff_8636_u16 low;
} __attribute__((packed));

struct sff_8636_upper_page_03_monitor_thresholds {
    struct sff_8636_upper_page_03_monitor_threshold alarm;
    struct sff_8636_upper_page_03_monitor_threshold warning;
} __attribute__((packed));

struct sff_8636_upper_page_03_free_side_thresholds {
    struct sff_8636_upper_page_03_monitor_thresholds temp; // 128-135
    uint8_t reserved0[8];                                  // 136-143
    struct sff_8636_upper_page_03_monitor_thresholds vcc;  // 128-135
    uint8_t reserved1[8];                                  // 152-159
    uint8_t vendor[16];                                    // 160-175
} __attribute__((packed));

struct sff_8636_upper_page_03_channel_thresholds {
    struct sff_8636_upper_page_03_monitor_thresholds rx_power; // 176-183
    struct sff_8636_upper_page_03_monitor_thresholds tx_bias;  // 184-191
    struct sff_8636_upper_page_03_monitor_thresholds tx_power; // 192-199
    uint8_t reserved[16];                                      // 200-215
    uint8_t vendor[8];                                         // 216-223
} __attribute__((packed));

struct sff_8636_upper_page_03_thresholds {
    struct sff_8636_upper_page_03_free_side_thresholds free_side; // 128-175
    struct sff_8636_upper_page_03_channel_thresholds channel;     // 176-223
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_03_indicators_equalizer_max {
    struct {
        uint8_t rx_emphasis  : 4; // [3:0]
        uint8_t tx_equalizer : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_indicators_equalizer_rx_output {
    struct {
        uint8_t rx_amplitude : 4; // [3:0]
        uint8_t rx_emphasis  : 2; // [5:4]
        uint8_t reserved     : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_03_indicators_equalizer {
    union sff_8636_upper_page_03_indicators_equalizer_max max;             // 224
    union sff_8636_upper_page_03_indicators_equalizer_rx_output rx_output; // 225
} __attribute__((packed));

union sff_8636_upper_page_03_indicators_supported {
    struct {
        uint8_t reserved0        : 1; // [0]
        uint8_t tx_dis_fast_mode : 1; // [1]
        uint8_t rx_los_fast_mode : 1; // [2]
        uint8_t tx_force_squelch : 1; // [3]
        uint8_t reserved1        : 2; // [5:4]
        uint8_t media_side_fec   : 1; // [6]
        uint8_t host_side_fec    : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_03_indicators {
    struct sff_8636_upper_page_03_indicators_equalizer equalizer; // 224-225
    uint8_t reserved[1];                                          // 226
    union sff_8636_upper_page_03_indicators_supported supported;  // 227
    uint8_t max_tc_stabilization_time;                            // 228
    uint8_t max_ctle_settling_time;                               // 229
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_03_channel_controls_fec {
    struct {
        uint8_t reserved          : 6; // [5:0]
        uint8_t media_side_enable : 1; // [6]
        uint8_t host_side_enable  : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_force_squelch {
    struct {
        uint8_t tx1      : 1; // [0]
        uint8_t tx2      : 1; // [1]
        uint8_t tx3      : 1; // [2]
        uint8_t tx4      : 1; // [3]
        uint8_t reserved : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_freeze {
    struct {
        uint8_t tx4      : 1; // [0]
        uint8_t tx3      : 1; // [1]
        uint8_t tx2      : 1; // [2]
        uint8_t tx1      : 1; // [3]
        uint8_t reserved : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_equalizer {
    struct {
        uint8_t tx2 : 4; // [3:0]
        uint8_t tx1 : 4; // [7:4]
        uint8_t tx4 : 4; // [11:8]
        uint8_t tx3 : 4; // [15:12]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_emphasis {
    struct {
        uint8_t rx2 : 4; // [3:0]
        uint8_t rx1 : 4; // [7:4]
        uint8_t rx4 : 4; // [11:8]
        uint8_t rx3 : 4; // [15:12]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_amplitude {
    struct {
        uint8_t rx2 : 4; // [3:0]
        uint8_t rx1 : 4; // [7:4]
        uint8_t rx4 : 4; // [11:8]
        uint8_t rx3 : 4; // [15:12]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_disable_squelch {
    struct {
        uint8_t tx1 : 1; // [0]
        uint8_t tx2 : 1; // [1]
        uint8_t tx3 : 1; // [2]
        uint8_t tx4 : 1; // [3]
        uint8_t rx1 : 1; // [4]
        uint8_t rx2 : 1; // [5]
        uint8_t rx3 : 1; // [6]
        uint8_t rx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_controls_host_side {
    struct {
        uint8_t tx1_equalizer : 1; // [0]
        uint8_t tx2_equalizer : 1; // [1]
        uint8_t tx3_equalizer : 1; // [2]
        uint8_t tx4_equalizer : 1; // [3]
        uint8_t rx1_output    : 1; // [4]
        uint8_t rx2_output    : 1; // [5]
        uint8_t rx3_output    : 1; // [6]
        uint8_t rx4_output    : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_03_channel_controls {
    union sff_8636_upper_page_03_channel_controls_fec fec;                         // 230
    union sff_8636_upper_page_03_channel_controls_force_squelch force_squelch;     // 231
    uint8_t reserved[1];                                                           // 232
    union sff_8636_upper_page_03_channel_controls_freeze freeze;                   // 233
    union sff_8636_upper_page_03_channel_controls_equalizer equalizer;             // 234-235
    union sff_8636_upper_page_03_channel_controls_emphasis emphasis;               // 236-237
    union sff_8636_upper_page_03_channel_controls_amplitude amplitude;             // 238-239
    union sff_8636_upper_page_03_channel_controls_disable_squelch disable_squelch; // 240
    union sff_8636_upper_page_03_channel_controls_host_side host_side;             // 241
} __attribute__((packed));

union sff_8636_upper_page_03_channel_monitor_masks_rx_power {
    struct {
        uint16_t rx2_low_warning  : 1; // [0]
        uint16_t rx2_high_warning : 1; // [1]
        uint16_t rx2_low_alarm    : 1; // [2]
        uint16_t rx2_high_alarm   : 1; // [3]
        uint16_t rx1_low_warning  : 1; // [4]
        uint16_t rx1_high_warning : 1; // [5]
        uint16_t rx1_low_alarm    : 1; // [6]
        uint16_t rx1_high_alarm   : 1; // [7]
        uint16_t rx4_low_warning  : 1; // [8]
        uint16_t rx4_high_warning : 1; // [9]
        uint16_t rx4_low_alarm    : 1; // [10]
        uint16_t rx4_high_alarm   : 1; // [11]
        uint16_t rx3_low_warning  : 1; // [12]
        uint16_t rx3_high_warning : 1; // [13]
        uint16_t rx3_low_alarm    : 1; // [14]
        uint16_t rx3_high_alarm   : 1; // [15]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_monitor_masks_tx_bias {
    struct {
        uint16_t tx2_low_warning  : 1; // [0]
        uint16_t tx2_high_warning : 1; // [1]
        uint16_t tx2_low_alarm    : 1; // [2]
        uint16_t tx2_high_alarm   : 1; // [3]
        uint16_t tx1_low_warning  : 1; // [4]
        uint16_t tx1_high_warning : 1; // [5]
        uint16_t tx1_low_alarm    : 1; // [6]
        uint16_t tx1_high_alarm   : 1; // [7]
        uint16_t tx4_low_warning  : 1; // [8]
        uint16_t tx4_high_warning : 1; // [9]
        uint16_t tx4_low_alarm    : 1; // [10]
        uint16_t tx4_high_alarm   : 1; // [11]
        uint16_t tx3_low_warning  : 1; // [12]
        uint16_t tx3_high_warning : 1; // [13]
        uint16_t tx3_low_alarm    : 1; // [14]
        uint16_t tx3_high_alarm   : 1; // [15]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_03_channel_monitor_masks_tx_power {
    struct {
        uint16_t tx2_low_warning  : 1; // [0]
        uint16_t tx2_high_warning : 1; // [1]
        uint16_t tx2_low_alarm    : 1; // [2]
        uint16_t tx2_high_alarm   : 1; // [3]
        uint16_t tx1_low_warning  : 1; // [4]
        uint16_t tx1_high_warning : 1; // [5]
        uint16_t tx1_low_alarm    : 1; // [6]
        uint16_t tx1_high_alarm   : 1; // [7]
        uint16_t tx4_low_warning  : 1; // [8]
        uint16_t tx4_high_warning : 1; // [9]
        uint16_t tx4_low_alarm    : 1; // [10]
        uint16_t tx4_high_alarm   : 1; // [11]
        uint16_t tx3_low_warning  : 1; // [12]
        uint16_t tx3_high_warning : 1; // [13]
        uint16_t tx3_low_alarm    : 1; // [14]
        uint16_t tx3_high_alarm   : 1; // [15]
    };
    uint16_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_03_channel_monitor_masks {
    union sff_8636_upper_page_03_channel_monitor_masks_rx_power rx_power; // 242-243
    union sff_8636_upper_page_03_channel_monitor_masks_tx_bias tx_bias;   // 244-245
    union sff_8636_upper_page_03_channel_monitor_masks_tx_power tx_power; // 246-247
    uint8_t reserved[4];                                                  // 248-251
} __attribute__((packed));

struct sff_8636_upper_page_03_channel {
    struct sff_8636_upper_page_03_channel_controls controls;           // 230-241
    struct sff_8636_upper_page_03_channel_monitor_masks monitor_masks; // 242-251
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_03 {
    struct sff_8636_upper_page_03_thresholds thresholds; // 128-223
    struct sff_8636_upper_page_03_indicators indicators; // 224-229
    struct sff_8636_upper_page_03_channel channel;       // 230-251
    uint8_t reserved[4];                                 // 252-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_03) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 03 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_03_H
