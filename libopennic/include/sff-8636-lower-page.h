#ifndef INCLUDE_SFF_8636_LOWER_PAGE_H
#define INCLUDE_SFF_8636_LOWER_PAGE_H

#include <assert.h>
#include <stdint.h>

#include "sff-8024.h"
#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
union sff_8636_lower_page_status_flags {
    struct {
        uint8_t data_not_ready : 1; // [0]
        uint8_t int_l          : 1; // [1]
        uint8_t flat_mem       : 1; // [2]
        uint8_t reserved       : 5; // [7:3]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_lower_page_status {
    uint8_t revision_compliance;                  // 1 (enum sff_8636_compliance_revision)
    union sff_8636_lower_page_status_flags flags; // 2
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_lower_page_interrupt_flags_los {
    struct {
        uint8_t rx1 : 1; // [0]
        uint8_t rx2 : 1; // [1]
        uint8_t rx3 : 1; // [2]
        uint8_t rx4 : 1; // [3]
        uint8_t tx1 : 1; // [4]
        uint8_t tx2 : 1; // [5]
        uint8_t tx3 : 1; // [6]
        uint8_t tx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_interrupt_flags_fault {
    struct {
        uint8_t tx1           : 1; // [0]
        uint8_t tx2           : 1; // [1]
        uint8_t tx3           : 1; // [2]
        uint8_t tx4           : 1; // [3]
        uint8_t tx1_equalizer : 1; // [4]
        uint8_t tx2_equalizer : 1; // [5]
        uint8_t tx3_equalizer : 1; // [6]
        uint8_t tx4_equalizer : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_interrupt_flags_lol {
    struct {
        uint8_t rx1 : 1; // [0]
        uint8_t rx2 : 1; // [1]
        uint8_t rx3 : 1; // [2]
        uint8_t rx4 : 1; // [3]
        uint8_t tx1 : 1; // [4]
        uint8_t tx2 : 1; // [5]
        uint8_t tx3 : 1; // [6]
        uint8_t tx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_interrupt_flags_temp {
    struct {
        uint8_t init_complete : 1; // [0]
        uint8_t tc_readiness  : 1; // [1]
        uint8_t reserved      : 2; // [3:2]
        uint8_t low_warning   : 1; // [4]
        uint8_t high_warning  : 1; // [5]
        uint8_t low_alarm     : 1; // [6]
        uint8_t high_alarm    : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_interrupt_flags_vcc {
    struct {
        uint8_t reserved      : 4; // [3:0]
        uint8_t low_warning   : 1; // [4]
        uint8_t high_warning  : 1; // [5]
        uint8_t low_alarm     : 1; // [6]
        uint8_t high_alarm    : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_interrupt_flags_rx_power {
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

union sff_8636_lower_page_interrupt_flags_tx_bias {
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

union sff_8636_lower_page_interrupt_flags_tx_power {
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

struct sff_8636_lower_page_interrupt_flags {
    struct {
        union sff_8636_lower_page_interrupt_flags_los los;           // 3
        union sff_8636_lower_page_interrupt_flags_fault fault;       // 4
        union sff_8636_lower_page_interrupt_flags_los lol;           // 5
    } channel_status;

    struct {
        union sff_8636_lower_page_interrupt_flags_temp temp;         // 6
        union sff_8636_lower_page_interrupt_flags_vcc vcc;           // 7
        uint8_t vendor;
    } free_side_monitor;

    struct {
        union sff_8636_lower_page_interrupt_flags_rx_power rx_power; // 9-10
        union sff_8636_lower_page_interrupt_flags_tx_bias tx_bias;   // 11-12
        union sff_8636_lower_page_interrupt_flags_tx_power tx_power; // 13-14
        uint8_t reserved[4];                                         // 15-18
        uint8_t vendor[3];                                           // 19-21
    } channel_monitor;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_lower_page_free_side_monitors {
    union sff_8636_u16 temp; // 22-23
    uint8_t reserved0[2];    // 24-25
    union sff_8636_u16 vcc;  // 26-27
    uint8_t reserved1[2];    // 28-29
    uint8_t vendor[4];       // 30-33
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_lower_page_channel_monitors {
    struct {
        union sff_8636_u16 rx1; // 34-35
        union sff_8636_u16 rx2; // 36-37
        union sff_8636_u16 rx3; // 38-39
        union sff_8636_u16 rx4; // 40-41
    } rx_power;

    struct {
        union sff_8636_u16 tx1; // 42-43
        union sff_8636_u16 tx2; // 44-45
        union sff_8636_u16 tx3; // 46-47
        union sff_8636_u16 tx4; // 48-49
    } tx_bias;

    struct {
        union sff_8636_u16 tx1; // 50-51
        union sff_8636_u16 tx2; // 52-53
        union sff_8636_u16 tx3; // 54-55
        union sff_8636_u16 tx4; // 56-57
    } tx_power;

    uint8_t reserved[16];        // 58-73
    uint8_t vendor[8];           // 74-81
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_lower_page_control_tx_disable {
    struct {
        uint8_t tx1      : 1; // [0]
        uint8_t tx2      : 1; // [1]
        uint8_t tx3      : 1; // [2]
        uint8_t tx4      : 1; // [3]
        uint8_t reserved : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_control_rate_select {
    struct {
        uint16_t rx1 : 2; // [1:0]
        uint16_t rx2 : 2; // [3:2]
        uint16_t rx3 : 2; // [5:4]
        uint16_t rx4 : 2; // [7:6]
        uint16_t tx1 : 2; // [9:8]
        uint16_t tx2 : 2; // [11:10]
        uint16_t tx3 : 2; // [13:12]
        uint16_t tx4 : 2; // [15:14]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_lower_page_control_app_select {
    struct {
        uint8_t table : 6; // [5:0]
        uint8_t mode  : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_lower_page_control_rx_app_select {
    union sff_8636_lower_page_control_app_select rx4; // 89
    union sff_8636_lower_page_control_app_select rx3; // 90
    union sff_8636_lower_page_control_app_select rx2; // 91
    union sff_8636_lower_page_control_app_select rx1; // 92
} __attribute__((packed));

struct sff_8636_lower_page_control_tx_app_select {
    union sff_8636_lower_page_control_app_select tx4; // 94
    union sff_8636_lower_page_control_app_select tx3; // 95
    union sff_8636_lower_page_control_app_select tx2; // 96
    union sff_8636_lower_page_control_app_select tx1; // 97
} __attribute__((packed));

union sff_8636_lower_page_control_config {
    struct {
        uint8_t power_override          : 1; // [0]
        uint8_t power_low_mode          : 1; // [1]
        uint8_t power_high_class_5_to_7 : 1; // [2]
        uint8_t power_high_class_8      : 1; // [3]
        uint8_t reserved                : 3; // [6:4]
        uint8_t sw_reset                : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_control_cdr {
    struct {
        uint8_t rx1 : 1; // [0]
        uint8_t rx2 : 1; // [1]
        uint8_t rx3 : 1; // [2]
        uint8_t rx4 : 1; // [3]
        uint8_t tx1 : 1; // [4]
        uint8_t tx2 : 1; // [5]
        uint8_t tx3 : 1; // [6]
        uint8_t tx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_control_signal {
    struct {
        uint8_t los_l_or_int_l    : 1; // [0]
        uint8_t tx_dis_or_lp_mode : 1; // [1]
        uint8_t reserved          : 6; // [7:2]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_lower_page_control {
    union sff_8636_lower_page_control_tx_disable tx_disable;        // 86
    union sff_8636_lower_page_control_rate_select rate_select;      // 87-88
    struct sff_8636_lower_page_control_rx_app_select rx_app_select; // 89-92
    union sff_8636_lower_page_control_config config;                // 93
    struct sff_8636_lower_page_control_tx_app_select tx_app_select; // 94-97
    union sff_8636_lower_page_control_cdr cdr;                      // 98
    union sff_8636_lower_page_control_signal signal;                // 99
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_lower_page_mask_los {
    struct {
        uint8_t rx1 : 1; // [0]
        uint8_t rx2 : 1; // [1]
        uint8_t rx3 : 1; // [2]
        uint8_t rx4 : 1; // [3]
        uint8_t tx1 : 1; // [4]
        uint8_t tx2 : 1; // [5]
        uint8_t tx3 : 1; // [6]
        uint8_t tx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_mask_fault {
    struct {
        uint8_t tx1           : 1; // [0]
        uint8_t tx2           : 1; // [1]
        uint8_t tx3           : 1; // [2]
        uint8_t tx4           : 1; // [3]
        uint8_t tx1_equalizer : 1; // [4]
        uint8_t tx2_equalizer : 1; // [5]
        uint8_t tx3_equalizer : 1; // [6]
        uint8_t tx4_equalizer : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_mask_lol {
    struct {
        uint8_t rx1 : 1; // [0]
        uint8_t rx2 : 1; // [1]
        uint8_t rx3 : 1; // [2]
        uint8_t rx4 : 1; // [3]
        uint8_t tx1 : 1; // [4]
        uint8_t tx2 : 1; // [5]
        uint8_t tx3 : 1; // [6]
        uint8_t tx4 : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_mask_temp {
    struct {
        uint8_t reserved0    : 1; // [0]
        uint8_t tc_readiness : 1; // [1]
        uint8_t reserved1    : 2; // [3:2]
        uint8_t low_warning  : 1; // [4]
        uint8_t high_warning : 1; // [5]
        uint8_t low_alarm    : 1; // [6]
        uint8_t high_alarm   : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_mask_vcc {
    struct {
        uint8_t reserved0    : 4; // [3:0]
        uint8_t low_warning  : 1; // [4]
        uint8_t high_warning : 1; // [5]
        uint8_t low_alarm    : 1; // [6]
        uint8_t high_alarm   : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_lower_page_mask {
    union sff_8636_lower_page_mask_los los;     // 100
    union sff_8636_lower_page_mask_fault fault; // 101
    union sff_8636_lower_page_mask_lol lol;     // 102
    union sff_8636_lower_page_mask_temp temp;   // 103
    union sff_8636_lower_page_mask_vcc vcc;     // 104
    uint8_t vendor[2];                          // 105-106
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_lower_page_properties_power {
    struct {
        uint8_t min_operating_voltage   : 3; // [2:0]
        uint8_t far_side_managed        : 1; // [3]
        uint8_t advanced_low_power_mode : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_properties_implementation {
    struct {
        uint8_t near_end_chan1 : 1; // [0]
        uint8_t near_end_chan2 : 1; // [1]
        uint8_t near_end_chan3 : 1; // [2]
        uint8_t near_end_chan4 : 1; // [3]
        uint8_t far_end        : 3; // [6:4]
        uint8_t reserved       : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_properties_max_duration {
    struct {
        uint8_t data_path_init : 4; // [3:0]
        uint8_t tx_turn_on     : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_lower_page_properties_xcvr {
    struct {
        uint8_t fiber_face_type : 2; // [1:0] (enum sff_8024_xcvr_fiber_face_type)
        uint8_t reserved        : 2; // [3:2]
        uint8_t sub_type        : 4; // [7:4] (enum sff_8024_xcvr_sub_type)
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_lower_page_properties {
    uint8_t max_power_consumption;                                                 // 107
    union sff_8636_u16 propagation_delay;                                          // 108-109
    union sff_8636_lower_page_properties_power power;                              // 110
    uint8_t pcie[2];                                                               // 111-112
    union sff_8636_lower_page_properties_implementation implementation;            // 113
    union sff_8636_lower_page_properties_max_duration max_duration;                // 114
    union sff_8636_f8 modsel_wait_time;                                            // 115
    uint8_t secondary_extended_compliance; /* enum sff_8024_extended_compliance */ // 116
    union sff_8636_lower_page_properties_xcvr xcvr;                                // 117
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_lower_page {
    uint8_t identifier; /* enum sff_8024_identifier */                // 0
    struct sff_8636_lower_page_status status;                         // 1-2
    struct sff_8636_lower_page_interrupt_flags interrupt_flags;       // 3-21
    struct sff_8636_lower_page_free_side_monitors free_side_monitors; // 22-33
    struct sff_8636_lower_page_channel_monitors channel_monitors;     // 34-81
    uint8_t reserved0[4];                                             // 82-85
    struct sff_8636_lower_page_control control;                       // 86-99
    struct sff_8636_lower_page_mask mask;                             // 100-106
    struct sff_8636_lower_page_properties properties;                 // 107-117
    uint8_t reserved1[1];                                             // 118
    uint8_t password_change[4];                                       // 119-122
    uint8_t password_entry[4];                                        // 123-126
    uint8_t page_select;                                              // 127
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_lower_page) == SFF_8636_PAGE_SIZE,
              "SFF-8636 lower page must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_LOWER_PAGE_H
