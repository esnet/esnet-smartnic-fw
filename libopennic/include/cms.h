#ifndef INCLUDE_CMS_H
#define INCLUDE_CMS_H

#include "cms_block.h"
#include <net/ethernet.h>
#include <pthread.h>
#include "sff-8636.h"
#include <stdbool.h>
#include <stdint.h>
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
struct cms {
    volatile struct cms_block* blk;
    pthread_mutex_t lock;
};

void cms_init(struct cms* cms);
void cms_destroy(struct cms* cms);

bool cms_is_ready(struct cms* cms);
bool cms_reset(struct cms* cms);
bool cms_enable(struct cms* cms);
void cms_disable(struct cms* cms);

//--------------------------------------------------------------------------------------------------
enum cms_profile {
    cms_profile_NONE,
    cms_profile_U200_U250,
    cms_profile_U280,
    cms_profile_U50,
    cms_profile_U55,
    cms_profile_UL3524,
    cms_profile_U45N,
    cms_profile_X3522,
};

#define _CMS_PROFILE_MASK(_profile) ((uint64_t)1 << (_profile))
#define CMS_PROFILE_MASK(_name)     _CMS_PROFILE_MASK(cms_profile_##_name)

const char* cms_profile_to_str(enum cms_profile profile);

//--------------------------------------------------------------------------------------------------
enum cms_card_info_config_mode {
    cms_card_info_config_mode_SLAVE_SERIAL_X1,
    cms_card_info_config_mode_SLAVE_SELECT_MAP_X8,
    cms_card_info_config_mode_SLAVE_SELECT_MAP_X16,
    cms_card_info_config_mode_SLAVE_SELECT_MAP_X32,
    cms_card_info_config_mode_JTAG_BOUNDARY_SCAN_X1,
    cms_card_info_config_mode_MASTER_SPI_X1,
    cms_card_info_config_mode_MASTER_SPI_X2,
    cms_card_info_config_mode_MASTER_SPI_X4,
    cms_card_info_config_mode_MASTER_SPI_X8,
    cms_card_info_config_mode_MASTER_BPI_X8,
    cms_card_info_config_mode_MASTER_BPI_X16,
    cms_card_info_config_mode_MASTER_SERIAL_X1,
    cms_card_info_config_mode_MASTER_SELECT_MAP_X8,
    cms_card_info_config_mode_MASTER_SELECT_MAP_X16,
};
const char* cms_card_info_config_mode_to_str(enum cms_card_info_config_mode config_mode);

enum cms_card_info_cage_type {
    cms_card_info_cage_type_QSFP,
    cms_card_info_cage_type_DSFP,
    cms_card_info_cage_type_SFP,
};
const char* cms_card_info_cage_type_to_str(enum cms_card_info_cage_type cage_type);

struct cms_card_info {
#define CMS_CARD_INFO_VALID_MASK(_idx)      (1 << (_idx))
#define CMS_CARD_INFO_IS_VALID(_mask, _idx) (((_mask) & CMS_CARD_INFO_VALID_MASK(_idx)) != 0)

    enum cms_profile profile;
    const char* name;
    const char* serial_number;
    const char* revision;
    const char* sc_version;

    bool fan_present;
    unsigned int total_power_avail;
    enum cms_card_info_config_mode config_mode;

    struct {
#define CMS_CARD_INFO_MAX_CAGES 4
#define CMS_CARD_INFO_CAGE_IS_VALID(_ci, _idx) \
    CMS_CARD_INFO_IS_VALID((_ci)->cage.valid_mask, _idx)

        enum cms_card_info_cage_type types[CMS_CARD_INFO_MAX_CAGES];
        unsigned int valid_mask;
    } cage;

    struct {
        struct {
#define CMS_CARD_INFO_MAX_LEGACY_MACS 4
#define CMS_CARD_INFO_LEGACY_MAC_IS_VALID(_ci, _idx) \
    CMS_CARD_INFO_IS_VALID((_ci)->mac.legacy.valid_mask, _idx)

            struct ether_addr addrs[CMS_CARD_INFO_MAX_LEGACY_MACS];
            unsigned int valid_mask;
        } legacy;

        struct {
            size_t count;
            struct ether_addr base;
        } block;
    } mac;
};

struct cms_card_info* cms_card_info_read(struct cms* cms);
void cms_card_info_free(struct cms_card_info* info);

//--------------------------------------------------------------------------------------------------
struct cms_module_id {
    uint8_t cage;
    uint8_t page;
    bool upper;

    bool cmis;
    uint8_t bank;

    bool sfp_diag;
};

#define CMS_MODULE_PAGE_SIZE 128
union cms_module_page {
    uint8_t u8[CMS_MODULE_PAGE_SIZE];
    uint32_t u32[CMS_MODULE_PAGE_SIZE / sizeof(uint32_t)];
    union sff_8636_page sff_8636;
};
static_assert(sizeof(union cms_module_page) == CMS_MODULE_PAGE_SIZE,
              "CMS module page size must be " STRINGIZE1(CMS_MODULE_PAGE_SIZE) " bytes.");
static_assert(CMS_MODULE_PAGE_SIZE % sizeof(uint32_t) == 0,
              "CMS module page size " STRINGIZE1(CMS_MODULE_PAGE_SIZE) " is not a multiple of 4.");

union cms_module_page* cms_module_page_read(struct cms* cms, const struct cms_module_id* id);
void cms_module_page_free(union cms_module_page* page);

bool cms_module_byte_read(struct cms* cms, const struct cms_module_id* id,
                          uint8_t offset, uint8_t* value);
bool cms_module_byte_write(struct cms* cms, const struct cms_module_id* id,
                           uint8_t offset, uint8_t value);

//--------------------------------------------------------------------------------------------------
enum cms_module_gpio_type {
    cms_module_gpio_type_SFP,
    cms_module_gpio_type_DSFP,
    cms_module_gpio_type_QSFP,
};

struct cms_module_gpio {
    enum cms_module_gpio_type type;
    union {
        struct {
            bool present;
        } sfp;
        struct {
            bool reset;
            bool low_power_mode;
            bool present;
            bool interrupt;
        } dsfp;
        struct {
            bool reset;
            bool low_power_mode;
            bool select;
            bool present;
            bool interrupt;
        } qsfp;
    };
};

bool cms_module_gpio_read(struct cms* cms, uint8_t cage, struct cms_module_gpio* gpio);
bool cms_module_gpio_write(struct cms* cms, uint8_t cage, const struct cms_module_gpio* gpio);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_CMS_H
