#ifndef INCLUDE_SFF_8636_UPPER_PAGE_00_H
#define INCLUDE_SFF_8636_UPPER_PAGE_00_H

#include <assert.h>
#include <stdint.h>

#include "sff-8024.h"
#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_extended_identifier {
    struct {
        uint8_t power_class_hi : 2; // [1:0]
        uint8_t rx_cdr         : 1; // [2]
        uint8_t tx_cdr         : 1; // [3]
        uint8_t clei_present   : 1; // [4]
        uint8_t power_class_8  : 1; // [5]
        uint8_t power_class_lo : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_compliance_ethernet {
    struct {
        uint8_t _40g_active_cable : 1; // [0]
        uint8_t _40gbase_lr4      : 1; // [1]
        uint8_t _40gbase_sr4      : 1; // [2]
        uint8_t _40gbase_cr4      : 1; // [3]
        uint8_t _10gbase_sr       : 1; // [4]
        uint8_t _10gbase_lr       : 1; // [5]
        uint8_t _10gbase_lrm      : 1; // [6]
        uint8_t extended          : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_sonet {
    struct {
        uint8_t oc48_sr  : 1; // [0]
        uint8_t oc48_ir  : 1; // [1]
        uint8_t oc48_lr  : 1; // [2]
        uint8_t reserved : 5; // [7:3]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_sas {
    struct {
        uint8_t reserved : 4; // [3:0]
        uint8_t _3_gbps  : 1; // [4]
        uint8_t _6_gbps  : 1; // [5]
        uint8_t _12_gbps : 1; // [6]
        uint8_t _24_gbps : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_gigabit_ethernet {
    struct {
        uint8_t _1000base_sx : 1; // [0]
        uint8_t _1000base_lx : 1; // [1]
        uint8_t _1000base_cx : 1; // [2]
        uint8_t _1000base_t  : 1; // [3]
        uint8_t reserved     : 4; // [7:4]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_fibre_link {
    struct {
        uint16_t tx_el_inter         : 1; // [0]
        uint16_t tx_lc               : 1; // [1]
        uint16_t reserved0           : 1; // [2]

        uint16_t length_medium       : 1; // [3]
        uint16_t length_long         : 1; // [4]
        uint16_t length_intermediate : 1; // [5]
        uint16_t length_short        : 1; // [6]
        uint16_t length_very_long    : 1; // [7]

        uint16_t reserved1           : 4; // [11:8]
        uint16_t tx_ll               : 1; // [12]
        uint16_t tx_sl               : 1; // [13]
        uint16_t tx_sn               : 1; // [14]
        uint16_t tx_el_intra         : 1; // [15]
    };
    uint16_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_fibre_media {
    struct {
        uint8_t sm  : 1; // [0]
        uint8_t om3 : 1; // [1]
        uint8_t m5  : 1; // [2]
        uint8_t m6  : 1; // [3]
        uint8_t tv  : 1; // [4]
        uint8_t mi  : 1; // [5]
        uint8_t tp  : 1; // [6]
        uint8_t tw  : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_compliance_fibre_speed {
    struct {
        uint8_t _100_MBps  : 1; // [0]
        uint8_t extended   : 1; // [1]
        uint8_t _200_MBps  : 1; // [2]
        uint8_t _3200_MBps : 1; // [3]
        uint8_t _400_MBps  : 1; // [4]
        uint8_t _1600_MBps : 1; // [5]
        uint8_t _800_MBps  : 1; // [6]
        uint8_t _1200_MBps : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_00_compliance_fibre {
    union sff_8636_upper_page_00_compliance_fibre_link link;   // 135-136
    union sff_8636_upper_page_00_compliance_fibre_media media; // 137
    union sff_8636_upper_page_00_compliance_fibre_speed speed; // 138
} __attribute__((packed));

struct sff_8636_upper_page_00_compliance {
    union sff_8636_upper_page_00_compliance_ethernet ethernet;                 // 131
    union sff_8636_upper_page_00_compliance_sonet sonet;                       // 132
    union sff_8636_upper_page_00_compliance_sas sas;                           // 133
    union sff_8636_upper_page_00_compliance_gigabit_ethernet gigabit_ethernet; // 134
    struct sff_8636_upper_page_00_compliance_fibre fibre;                      // 135-138
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_extended_rate_select {
    struct {
        uint8_t version  : 2; // [1:0]
        uint8_t reserved : 6; // [7:2]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_00_link_length {
    uint8_t smf;     // 142
    uint8_t om3;     // 143
    uint8_t om2;     // 144
    uint8_t om1;     // 145
    uint8_t passive; // 146
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_device_technology {
    struct {
        uint8_t tx_tunable                : 1; // [0]
        uint8_t apd_detector              : 1; // [1]
        uint8_t cooled_tx                 : 1; // [2]
        uint8_t active_wavelength_control : 1; // [3]
        uint8_t tx_technology             : 4; // [7:4] (enum sff_8636_tx_technology)
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_extended_module {
    struct {
        uint8_t sdr      : 1; // [0]
        uint8_t ddr      : 1; // [1]
        uint8_t qdr      : 1; // [2]
        uint8_t fdr      : 1; // [3]
        uint8_t edr      : 1; // [4]
        uint8_t hdr      : 1; // [5]
        uint8_t reserved : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_cable {
    struct {
        union sff_8636_u16 value;     // 186-187
        union sff_8636_u16 tolerance; // 188-189
    } wavelength;
    struct {
        uint8_t at_2_5_GHz;           // 186
        uint8_t at_5_0_GHz;           // 187
        uint8_t at_7_0_GHz;           // 188
        uint8_t at_12_9_GHz;          // 189
    } attenuation;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_options_equalizer {
    struct {
        uint8_t rx_amplitude_programmable      : 1; // [0]
        uint8_t rx_emphasis_programmable       : 1; // [1]
        uint8_t tx_input_programmable          : 1; // [2]
        uint8_t tx_input_auto_adaptive         : 1; // [3]
        uint8_t tx_input_freeze                : 1; // [4]
        uint8_t int_l_or_los_l_configurable    : 1; // [5]
        uint8_t lp_mode_or_tx_dis_configurable : 1; // [6]
        uint8_t reserved                       : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_options_cdr {
    struct {
        uint8_t tx_squelch         : 1; // [0]
        uint8_t tx_squelch_disable : 1; // [1]
        uint8_t rx_output_disable  : 1; // [2]
        uint8_t rx_squelch_disable : 1; // [3]
        uint8_t rx_lol             : 1; // [4]
        uint8_t tx_lol             : 1; // [5]
        uint8_t rx_on_off_control  : 1; // [6]
        uint8_t tx_on_off_control  : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_00_options_device {
    struct {
        uint8_t pages_20_21 : 1; // [0]
        uint8_t tx_los      : 1; // [1]
        uint8_t tx_squelch  : 1; // [2]
        uint8_t tx_fault    : 1; // [3]
        uint8_t tx_disable  : 1; // [4]
        uint8_t rate_select : 1; // [5]
        uint8_t page_01     : 1; // [6]
        uint8_t page_02     : 1; // [7]
    };
    uint8_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_00_options {
    union sff_8636_upper_page_00_options_equalizer equalizer; // 193
    union sff_8636_upper_page_00_options_cdr cdr;             // 194
    union sff_8636_upper_page_00_options_device device;       // 195
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_date_code {
    struct {
        char year[2];   // 212-213
        char month[2];  // 214-215
        char day[2];    // 216-217
        char vendor[2]; // 218-219
    };
    char _v[8];
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_diag_type {
    struct {
        uint8_t reserved0          : 2; // [1:0]
        uint8_t tx_power_supported : 1; // [2]
        uint8_t rx_power_average   : 1; // [3]
        uint8_t vcc_monitoring     : 1; // [4]
        uint8_t temp_monitoring    : 1; // [5]
        uint8_t reserved1          : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_00_enhanced_options {
    struct {
        uint8_t sw_reset       : 1; // [0]
        uint8_t tc_readiness   : 1; // [1]
        uint8_t reserved0      : 1; // [2]
        uint8_t rate_selection : 1; // [3]
        uint8_t init_complete  : 1; // [4]
        uint8_t reserved1      : 3; // [7:5]
    };
    uint8_t _v;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_00 {
    uint8_t identifier; /* enum sff_8024_identifier */                      // 128
    union sff_8636_upper_page_00_extended_identifier extended_identifier;   // 129
    uint8_t connector_type; /* enum sff_8024_connector_type */              // 130
    struct sff_8636_upper_page_00_compliance compliance;                    // 131-138
    uint8_t encoding; /* enum sff_8024_encoding */                          // 139
    uint8_t baud_rate;                                                      // 140
    union sff_8636_upper_page_00_extended_rate_select extended_rate_select; // 141
    struct sff_8636_upper_page_00_link_length link_length;                  // 142-146
    union sff_8636_upper_page_00_device_technology device_technology;       // 147
    char vendor_name[16];                                                   // 148-163
    union sff_8636_upper_page_00_extended_module extended_module;           // 164
    uint8_t vendor_oui[3];                                                  // 165-167
    char vendor_pn[16];                                                     // 168-183
    char vendor_rev[2];                                                     // 184-185
    union sff_8636_upper_page_00_cable cable;                               // 186-189
    uint8_t max_case_temp;                                                  // 190
    uint8_t cc_base;                                                        // 191
    uint8_t link_codes; /* enum sff_8024_extended_compliance */             // 192
    struct sff_8636_upper_page_00_options options;                          // 193-195
    char vendor_sn[16];                                                     // 196-211
    union sff_8636_upper_page_00_date_code date_code;                       // 212-219
    union sff_8636_upper_page_00_diag_type diag_type;                       // 220
    union sff_8636_upper_page_00_enhanced_options enhanced_options;         // 221
    uint8_t extended_baud_rate;                                             // 222
    uint8_t cc_ext;                                                         // 223
    uint8_t vendor[32];                                                     // 224-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_00) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 00 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_00_H
