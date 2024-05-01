#include "cms.h"

#include "array_size.h"
#include <assert.h>
#include <errno.h>
#include "memory-barriers.h"
#include <netinet/ether.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//--------------------------------------------------------------------------------------------------
#define log_err(_rv, _format, _args...) \
    fprintf(stderr, "ERROR(%s)[%d (%s)]: " _format "\n", __func__, _rv, strerror(_rv),## _args)
#define log_panic(_rv, _format, _args...) \
    {log_err(_rv, _format,## _args); exit(EXIT_FAILURE);}

//--------------------------------------------------------------------------------------------------
static inline void cms_lock(struct cms* cms) {
    int rv = pthread_mutex_lock(&cms->lock);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_lock failed");
    }
}

static inline void cms_unlock(struct cms* cms) {
    int rv = pthread_mutex_unlock(&cms->lock);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_unlock failed");
    }
}

//--------------------------------------------------------------------------------------------------
void cms_init(struct cms* cms) {
    int rv = pthread_mutex_init(&cms->lock, NULL);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_init failed");
    }
}

//--------------------------------------------------------------------------------------------------
void cms_destroy(struct cms* cms) {
    int rv = pthread_mutex_destroy(&cms->lock);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_destroy failed");
    }
}

//--------------------------------------------------------------------------------------------------
const char* cms_profile_to_str(enum cms_profile profile) {
    const char* str = "UNKNOWN";
    switch (profile) {
#define CASE_PROFILE(_name) case cms_profile_##_name: str = #_name; break
    CASE_PROFILE(NONE);
    CASE_PROFILE(U200_U250);
    CASE_PROFILE(U280);
    CASE_PROFILE(U50);
    CASE_PROFILE(U55);
    CASE_PROFILE(UL3524);
    CASE_PROFILE(U45N);
    CASE_PROFILE(X3522);
#undef CASE_PROFILE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
static enum cms_profile cms_profile_get(volatile struct cms_block* cms) {
    enum cms_profile profile = cms_profile_NONE;
    switch (cms->profile_name_reg) {
    // Codes are ASCII, but limited to 4 bytes, so map them to an enum for internal use.
    case 0x55325858: profile = cms_profile_U200_U250; break;
    case 0x55323830: profile = cms_profile_U280; break;
    case 0x55353041: profile = cms_profile_U50; break;
    case 0x5535354e: profile = cms_profile_U55; break;
    case 0x55333234: profile = cms_profile_UL3524; break;
    case 0x55323641: profile = cms_profile_U45N; break;
    case 0x58334100: profile = cms_profile_X3522; break;
    }

    return profile;
}

//--------------------------------------------------------------------------------------------------
static bool cms_reg_map_is_ready(volatile struct cms_block* cms) {
#define ITER_TIMEOUT_MS (2 * 1000)
#define ITER_DELAY_MS   100
#define MAX_ITER        (ITER_TIMEOUT_MS / ITER_DELAY_MS)

    for (unsigned int n = 0; n < MAX_ITER; ++n) {
        union cms_host_status2_reg status = {._v = cms->host_status2_reg._v};
        if (status.reg_map_ready) {
            return true;
        }
        usleep(ITER_DELAY_MS * 1000);
    }

    return false;

#undef ITER_TIMEOUT_MS
#undef ITER_DELAY_MS
#undef MAX_ITER
}

//--------------------------------------------------------------------------------------------------
enum cms_sc_mode {
    cms_sc_mode_UNKNOWN,
    cms_sc_mode_NORMAL,
    cms_sc_mode_BSL_MODE_UNSYNCED,
    cms_sc_mode_BSL_MODE_SYNCED,
    cms_sc_mode_BSL_MODE_SYNCED_SC_NOT_UPGRADABLE,
    cms_sc_mode_NORMAL_SC_NOT_UPGRADABLE,
};

static bool cms_sc_is_ready(volatile struct cms_block* cms) {
#define ITER_TIMEOUT_MS (2 * 1000)
#define ITER_DELAY_MS   100
#define MAX_ITER        (ITER_TIMEOUT_MS / ITER_DELAY_MS)

    for (unsigned int n = 0; n < MAX_ITER; ++n) {
        union cms_status_reg status = {._v = cms->status_reg._v};
        switch (status.sat_ctrl_mode) {
        case cms_sc_mode_NORMAL:
        case cms_sc_mode_NORMAL_SC_NOT_UPGRADABLE:
            return true;

        default:
            break;
        }
        usleep(ITER_DELAY_MS * 1000);
    }

    return false;

#undef ITER_TIMEOUT_MS
#undef ITER_DELAY_MS
#undef MAX_ITER
}

//--------------------------------------------------------------------------------------------------
enum cms_sc_error {
    cms_sc_error_NONE,
    cms_sc_error_SAT_COMMS_CHKUM_ERR,
    cms_sc_error_SAT_COMMS_EOP_ERR,
    cms_sc_error_SAT_COMMS_SOP_ERR,
    cms_sc_error_SAT_COMMS_ESQ_SEQ_ERR,
    cms_sc_error_SAT_COMMS_BAD_MSG_ID,
    cms_sc_error_SAT_COMMS_BAD_VERSION,
    cms_sc_error_SAT_COMMS_RX_BUF_OVERFLOW,
    cms_sc_error_SAT_COMMS_BAD_SENSOR_ID,
    cms_sc_error_SAT_COMMS_NS_MSG_ID,
    cms_sc_error_SAT_COMMS_SC_FUN_ERR,
    cms_sc_error_SAT_COMMS_FAIL_TO_EN_BSL,
};

static const char* cms_sc_error_to_str(enum cms_sc_error error) {
    const char* str = "UNKNOWN";
    switch (error) {
#define CASE_SC_ERROR(_name) case cms_sc_error_##_name: str = #_name; break
    CASE_SC_ERROR(NONE);
    CASE_SC_ERROR(SAT_COMMS_CHKUM_ERR);
    CASE_SC_ERROR(SAT_COMMS_EOP_ERR);
    CASE_SC_ERROR(SAT_COMMS_SOP_ERR);
    CASE_SC_ERROR(SAT_COMMS_ESQ_SEQ_ERR);
    CASE_SC_ERROR(SAT_COMMS_BAD_MSG_ID);
    CASE_SC_ERROR(SAT_COMMS_BAD_VERSION);
    CASE_SC_ERROR(SAT_COMMS_RX_BUF_OVERFLOW);
    CASE_SC_ERROR(SAT_COMMS_BAD_SENSOR_ID);
    CASE_SC_ERROR(SAT_COMMS_NS_MSG_ID);
    CASE_SC_ERROR(SAT_COMMS_SC_FUN_ERR);
    CASE_SC_ERROR(SAT_COMMS_FAIL_TO_EN_BSL);
#undef CASE_SC_ERROR
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
static bool cms_error_is_cleared(volatile struct cms_block* cms) {
#define ITER_TIMEOUT_MS (2 * 1000)
#define ITER_DELAY_MS   100
#define MAX_ITER        (ITER_TIMEOUT_MS / ITER_DELAY_MS)

    for (unsigned int n = 0; n < MAX_ITER; ++n) {
        union cms_control_reg control = {._v = cms->control_reg._v};
        if (!control.reset_error_reg) {
            return true;
        }
        usleep(ITER_DELAY_MS * 1000);
    }

    return false;

#undef ITER_TIMEOUT_MS
#undef ITER_DELAY_MS
#undef MAX_ITER
}

//--------------------------------------------------------------------------------------------------
static void cms_error_clear(volatile struct cms_block* cms) {
    union cms_control_reg control = {._v = cms->control_reg._v};
    control.reset_error_reg = 1;
    cms->control_reg._v = control._v;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static bool cms_mailbox_is_ready(volatile struct cms_block* cms) {
#define ITER_TIMEOUT_MS (2 * 1000)
#define ITER_DELAY_MS   100
#define MAX_ITER        (ITER_TIMEOUT_MS / ITER_DELAY_MS)

    for (unsigned int n = 0; n < MAX_ITER; ++n) {
        union cms_control_reg control = {._v = cms->control_reg._v};
        if (!control.mailbox_msg_status) {
            return true;
        }
        usleep(ITER_DELAY_MS * 1000);
    }

    return false;

#undef ITER_TIMEOUT_MS
#undef ITER_DELAY_MS
#undef MAX_ITER
}

//--------------------------------------------------------------------------------------------------
bool cms_is_ready(struct cms* cms) {
    if (!cms_reg_map_is_ready(cms->blk)) {
        log_err(EBUSY, "regmap ready timeout");
        return false;
    }

    if (!cms_sc_is_ready(cms->blk)) {
        log_err(EBUSY, "satellite controller ready timeout");
        return false;
    }

    if (!cms_mailbox_is_ready(cms->blk)) {
        log_err(EBUSY, "mailbox ready timeout");
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static void cms_microblaze_reset_assert(volatile struct cms_block* cms) {
    cms->mb_resetn_reg = 0;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static void cms_microblaze_reset_deassert(volatile struct cms_block* cms) {
    cms->mb_resetn_reg = 1;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static void cms_microblaze_reset_release(volatile struct cms_block* cms) {
    if (cms->mb_resetn_reg == 0) {
        cms_microblaze_reset_deassert(cms);

        /*
         * Sleep for 5s to allow the embedded microblaze to boot fully. Without this, register
         * reads can return old/stale values while the microblaze is booting.
         * TODO: can this be detected through polling rather than needing a fixed timeout?
         */
        usleep(5 * 1000 * 1000);
    }
}

//--------------------------------------------------------------------------------------------------
static void cms_microblaze_reset(volatile struct cms_block* cms) {
    cms_microblaze_reset_assert(cms);
    usleep(10 * 1000);
    cms_microblaze_reset_release(cms);
}

//--------------------------------------------------------------------------------------------------
bool cms_reset(struct cms* cms) {
    cms_lock(cms);
    cms_microblaze_reset(cms->blk);
    cms_unlock(cms);

    return cms_is_ready(cms);
}

//--------------------------------------------------------------------------------------------------
bool cms_enable(struct cms* cms) {
    cms_lock(cms);
    cms_microblaze_reset_release(cms->blk);
    cms_unlock(cms);

    return cms_is_ready(cms);
}

//--------------------------------------------------------------------------------------------------
void cms_disable(struct cms* cms) {
    cms_lock(cms);
    cms_microblaze_reset_assert(cms->blk);
    cms_unlock(cms);
}

//--------------------------------------------------------------------------------------------------
enum cms_mailbox_opcode {
    // https://docs.amd.com/r/en-US/pg348-cms-subsystem/Satellite-Controller-Firmware-Update
    cms_mailbox_opcode_SC_FW_SEC = 0x01,
    cms_mailbox_opcode_SC_FW_DATA = 0x02,
    cms_mailbox_opcode_SC_FW_REBOOT = 0x03,
    cms_mailbox_opcode_SC_FW_ERASE = 0x05,

    // https://docs.amd.com/r/en-US/pg348-cms-subsystem/Card-Information-Reporting
    cms_mailbox_opcode_CARD_INFO_REQ = 0x04,

    // https://docs.amd.com/r/en-US/pg348-cms-subsystem/Network-Plug-in-Module-Management
    cms_mailbox_opcode_READ_MODULE_LOW_SPEED_IO = 0x0d,
    cms_mailbox_opcode_WRITE_MODULE_LOW_SPEED_IO = 0x0e,
    cms_mailbox_opcode_BLOCK_READ_MODULE_I2C = 0x0b,
    cms_mailbox_opcode_BYTE_READ_MODULE_I2C = 0x0f,
    cms_mailbox_opcode_BYTE_WRITE_MODULE_I2C = 0x10,
};

struct cms_mailbox_header {
    uint32_t length   : 12; // [11:0] (in bytes)
    uint32_t reserved : 12; // [23:12]
    uint32_t opcode   : 8;  // [31:24] (value from enum cms_mailbox_opcode)
} __attribute__((packed));

struct cms_mailbox {
    struct cms_mailbox_header header;
    uint32_t payload[0];
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
static inline volatile struct cms_mailbox* cms_mailbox_get(volatile struct cms_block* cms) {
    return (volatile void*)&cms->reg_map_id_reg + cms->host_msg_offset_reg;
}

//--------------------------------------------------------------------------------------------------
static volatile struct cms_mailbox* cms_mailbox_post(volatile struct cms_block* cms,
                                                     const struct cms_mailbox* req,
                                                     size_t nwords) {
    if (!cms_mailbox_is_ready(cms)) {
        log_err(EBUSY, "mailbox ready timeout");
        return NULL;
    }

    cms_error_clear(cms);
    if (!cms_error_is_cleared(cms)) {
        log_err(EBUSY, "error clear timeout");
        return NULL;
    }

    volatile struct cms_mailbox* mailbox = cms_mailbox_get(cms);
    mailbox->header = req->header;
    for (unsigned int n = 0; n < nwords; ++n) {
        mailbox->payload[n] = req->payload[n];
    }
    barrier();

    union cms_control_reg control = {._v = cms->control_reg._v};
    control.mailbox_msg_status = 1;
    cms->control_reg._v = control._v;
    barrier();

    if (!cms_mailbox_is_ready(cms)) {
        log_err(EBUSY, "mailbox done timeout");
        return NULL;
    }

    union cms_error_reg error = {._v = cms->error_reg._v};
    if (error.pkt_error) {
        log_err(EIO, "packet error");
        return NULL;
    }

    if (error.sat_ctrl_err) {
        log_err(EIO, "satellite controller error %u (%s)", error.sat_ctrl_err_code,
                cms_sc_error_to_str(error.sat_ctrl_err_code));
        return NULL;
    }

    return mailbox;
}

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_CARD_INFO_REQ-0x04
enum cms_card_info_key {
    cms_card_info_key_CARD_SN = 0x21,
    cms_card_info_key_MAC_ADDRESS0,
    cms_card_info_key_MAC_ADDRESS1,
    cms_card_info_key_MAC_ADDRESS2,
    cms_card_info_key_MAC_ADDRESS3,
    cms_card_info_key_CARD_REV,
    cms_card_info_key_CARD_NAME,
    cms_card_info_key_SAT_VERSION,
    cms_card_info_key_TOTAL_POWER_AVAIL,
    cms_card_info_key_FAN_PRESENCE,
    cms_card_info_key_CONFIG_MODE,

    cms_card_info_key_NEW_MAC_SCHEME = 0x4b,

    cms_card_info_key_CAGE_TYPE_00 = 0x50,
    cms_card_info_key_CAGE_TYPE_01,
    cms_card_info_key_CAGE_TYPE_02,
    cms_card_info_key_CAGE_TYPE_03,
};

struct cms_card_info_field_header {
    uint8_t key;    // enum cms_card_info_key
    uint8_t length; // Length of payload (in bytes) that follows the header.
} __attribute__((packed));

struct cms_card_info_field {
    struct cms_card_info_field_header header;
    union {
        char str[0]; // Length from field header.
        uint8_t u8;
    } value;
} __attribute__((packed));

struct cms_card_info_new_mac_scheme {
    uint8_t count;
    uint8_t reserved;
    uint8_t base[ETH_ALEN];
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
static size_t cms_card_info_parse_field(struct cms_card_info_field* field,
                                        struct cms_card_info* info) {
    switch (field->header.key) {
    case cms_card_info_key_CARD_SN:
        info->serial_number = field->value.str;
        break;

    case cms_card_info_key_CARD_REV:
        info->revision = field->value.str;
        break;

    case cms_card_info_key_CARD_NAME:
        info->name = field->value.str;
        break;

    case cms_card_info_key_SAT_VERSION:
        info->sc_version = field->value.str;
        break;

    case cms_card_info_key_TOTAL_POWER_AVAIL:
        if (field->header.length != 1) {
            break;
        }

        switch (field->value.u8) {
        case 0: info->total_power_avail = 75; break;
        case 1: info->total_power_avail = 150; break;
        case 2: info->total_power_avail = 225; break;
        case 4: info->total_power_avail = 300; break;
        }
        break;

    case cms_card_info_key_FAN_PRESENCE:
        if (field->header.length != 1) {
            break;
        }

        info->fan_present = field->value.u8 != '0';
        break;

    case cms_card_info_key_CONFIG_MODE:
        if (field->header.length != 1) {
            break;
        }

        info->config_mode = field->value.u8;
        break;

    case cms_card_info_key_MAC_ADDRESS0:
    case cms_card_info_key_MAC_ADDRESS1:
    case cms_card_info_key_MAC_ADDRESS2:
    case cms_card_info_key_MAC_ADDRESS3: {
        if (field->header.length != 18) {
            break;
        }

        unsigned int n = field->header.key - cms_card_info_key_MAC_ADDRESS0;
        if (n < ARRAY_SIZE(info->mac.legacy.addrs)) {
            ether_aton_r(field->value.str, &info->mac.legacy.addrs[n]);
            info->mac.legacy.valid_mask |= CMS_CARD_INFO_VALID_MASK(n);
        }
        break;
    }

    case cms_card_info_key_NEW_MAC_SCHEME: {
        if (field->header.length != sizeof(struct cms_card_info_new_mac_scheme)) {
            break;
        }

        struct cms_card_info_new_mac_scheme* mac = (typeof(mac))&field->value.u8;
        info->mac.block.count = mac->count;
        memcpy(info->mac.block.base.ether_addr_octet, mac->base, ETH_ALEN);
        break;
    }

    case cms_card_info_key_CAGE_TYPE_00:
    case cms_card_info_key_CAGE_TYPE_01:
    case cms_card_info_key_CAGE_TYPE_02:
    case cms_card_info_key_CAGE_TYPE_03: {
        if (field->header.length != 1) {
            break;
        }

        unsigned int n = field->header.key - cms_card_info_key_CAGE_TYPE_00;
        if (n < ARRAY_SIZE(info->cage.types)) {
            info->cage.types[n] = field->value.u8;
            info->cage.valid_mask |= CMS_CARD_INFO_VALID_MASK(n);
        }
        break;
    }

    default:
        break;
    }

    return sizeof(field->header) + field->header.length;
}

//--------------------------------------------------------------------------------------------------
static struct cms_card_info* cms_card_info_parse(volatile struct cms_mailbox* mailbox) {
    size_t length = mailbox->header.length;
    size_t nwords = (length + sizeof(uint32_t) - 1) / sizeof(uint32_t);

    struct cms_card_info* info = calloc(1, sizeof(*info) + nwords * sizeof(uint32_t));
    if (info == NULL) {
        return NULL;
    }
    uint8_t* payload = (typeof(payload))&info[1];

    uint32_t* words = (typeof(words))payload;
    for (unsigned int n = 0; n < nwords; ++n) {
        words[n] = mailbox->payload[n];
    }

    size_t len = 0;
    while (len < length) {
        struct cms_card_info_field* field = (typeof(field))&payload[len];
        len += cms_card_info_parse_field(field, info);
    }

    return info;
}

//--------------------------------------------------------------------------------------------------
struct cms_card_info* cms_card_info_read(struct cms* cms) {
    struct cms_mailbox req = {
        .header = {
            .opcode = cms_mailbox_opcode_CARD_INFO_REQ,
        },
    };

    struct cms_card_info* info = NULL;
    cms_lock(cms);

    volatile struct cms_mailbox* mailbox = cms_mailbox_post(cms->blk, &req, 0);
    if (mailbox == NULL) {
        goto unlock;
    }

    info = cms_card_info_parse(mailbox);
    if (info == NULL) {
        goto unlock;
    }

    info->profile = cms_profile_get(cms->blk);

unlock:
    cms_unlock(cms);
    return info;
}

//--------------------------------------------------------------------------------------------------
void cms_card_info_free(struct cms_card_info* info) {
    free(info);
}

//--------------------------------------------------------------------------------------------------
const char* cms_card_info_config_mode_to_str(enum cms_card_info_config_mode config_mode) {
    const char* str = "UNKNOWN";
    switch (config_mode) {
#define CASE_CONFIG_MODE(_name) case cms_card_info_config_mode_##_name: str = #_name; break
    CASE_CONFIG_MODE(SLAVE_SERIAL_X1);
    CASE_CONFIG_MODE(SLAVE_SELECT_MAP_X8);
    CASE_CONFIG_MODE(SLAVE_SELECT_MAP_X16);
    CASE_CONFIG_MODE(SLAVE_SELECT_MAP_X32);
    CASE_CONFIG_MODE(JTAG_BOUNDARY_SCAN_X1);
    CASE_CONFIG_MODE(MASTER_SPI_X1);
    CASE_CONFIG_MODE(MASTER_SPI_X2);
    CASE_CONFIG_MODE(MASTER_SPI_X4);
    CASE_CONFIG_MODE(MASTER_SPI_X8);
    CASE_CONFIG_MODE(MASTER_BPI_X8);
    CASE_CONFIG_MODE(MASTER_BPI_X16);
    CASE_CONFIG_MODE(MASTER_SERIAL_X1);
    CASE_CONFIG_MODE(MASTER_SELECT_MAP_X8);
    CASE_CONFIG_MODE(MASTER_SELECT_MAP_X16);
#undef CASE_CONFIG_MODE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* cms_card_info_cage_type_to_str(enum cms_card_info_cage_type cage_type) {
    const char* str = "UNKNOWN";
    switch (cage_type) {
#define CASE_CAGE_TYPE(_name) case cms_card_info_cage_type_##_name: str = #_name; break
    CASE_CAGE_TYPE(QSFP);
    CASE_CAGE_TYPE(DSFP);
    CASE_CAGE_TYPE(SFP);
#undef CASE_CAGE_TYPE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
struct cms_op_module_i2c_select {
    // Word 1
    struct {
        uint32_t cage      : 2;  // [1:0]
        uint32_t reserved0 : 30; // [31:2]
    };

    // Word 2
    struct {
        uint32_t page      : 8;  // [7:0]
        uint32_t reserved1 : 24; // [31:8]
    };

    // Word 3
    struct {
        uint32_t upper           : 1;  // [0]
        uint32_t reserved2       : 15; // [15:1]
        uint32_t sfp_diag        : 1;  // [16]
        uint32_t cmis_bank_valid : 1;  // [17]
        uint32_t cmis_bank       : 5;  // [22:18]
        uint32_t reserved3       : 9;  // [31:23]
    };
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_BLOCK_READ_MODULE_I2C-0x0B
union cms_op_block_read_module_i2c {
    struct cms_mailbox overlay;
    struct {
        // Word 0
        struct cms_mailbox_header header;

        struct {
            // Word 1-3
            struct cms_op_module_i2c_select select;
        } request;

        struct {
            // Word 4
            uint32_t size; // (in bytes)

            // Word 5-n
            uint32_t payload[0];
        } response;
    } message;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
union cms_module_page* cms_module_page_read(struct cms* cms, const struct cms_module_id* id) {
    union cms_op_block_read_module_i2c op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_BLOCK_READ_MODULE_I2C,
            },
            .request = {
                .select = {
                    .cage = id->cage,
                    .page = id->upper ? id->page : 0, // Lower memory mapping is fixed.
                    .upper = id->upper ? 1 : 0, // Upper memory mapping is paged (except SFP).
                    .sfp_diag = id->sfp_diag ? 1 : 0,
                    .cmis_bank_valid = id->cmis ? 1 : 0,
                    .cmis_bank = id->cmis ? id->bank : 0,
                },
            },
        },
    };

    union cms_module_page* page = NULL;
    cms_lock(cms);

    volatile struct cms_mailbox* mailbox = cms_mailbox_post(
        cms->blk, &op.overlay, sizeof(op.message.request) / sizeof(uint32_t));
    if (mailbox == NULL) {
        goto unlock;
    }

    volatile union cms_op_block_read_module_i2c* mb = (typeof(mb))mailbox;
    size_t size = mb->message.response.size;
    if (size != sizeof(page->u8)) {
        //log_err(EINVAL, "Got %zu bytes, expected %zu", size, sizeof(page->u8));
        goto unlock;
    }

    page = malloc(sizeof(*page));
    if (page == NULL) {
        goto unlock;
    }

    for (unsigned int n = 0; n < size / sizeof(uint32_t); ++n) {
        page->u32[n] = mb->message.response.payload[n];
    }

unlock:
    cms_unlock(cms);
    return page;
}

//--------------------------------------------------------------------------------------------------
void cms_module_page_free(union cms_module_page* page) {
    free(page);
}

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_BYTE_READ_MODULE_I2C-0x0F
union cms_op_byte_read_module_i2c {
    struct cms_mailbox overlay;
    struct {
        // Word 0
        struct cms_mailbox_header header;

        struct {
            // Word 1-3
            struct cms_op_module_i2c_select select;

            // Word 4
            struct {
                uint32_t offset   : 8;  // [7:0]
                uint32_t reserved : 24; // [31:8]
            };
        } request;

        struct {
            // Word 5
            struct {
                uint32_t value    : 8;  // [7:0]
                uint32_t reserved : 24; // [31:8]
            };
        } response;
    } message;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
bool cms_module_byte_read(struct cms* cms,
                          const struct cms_module_id* id,
                          uint8_t offset,
                          uint8_t* value) {
    bool upper = offset >= CMS_MODULE_PAGE_SIZE;
    union cms_op_byte_read_module_i2c op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_BYTE_READ_MODULE_I2C,
            },
            .request = {
                .select = {
                    .cage = id->cage,
                    .page = upper ? id->page : 0, // Lower memory mapping is fixed.
                    .upper = upper, // Upper memory mapping is paged (except SFP).
                    .sfp_diag = id->sfp_diag ? 1 : 0,
                    .cmis_bank_valid = id->cmis ? 1 : 0,
                    .cmis_bank = id->cmis ? id->bank : 0,
                },
                .offset = offset,
            },
        },
    };

    cms_lock(cms);
    volatile struct cms_mailbox* mailbox = cms_mailbox_post(
        cms->blk, &op.overlay, sizeof(op.message.request) / sizeof(uint32_t));
    if (mailbox != NULL) {
        volatile union cms_op_byte_read_module_i2c* mb = (typeof(mb))mailbox;
        *value = mb->message.response.value;
    }
    cms_unlock(cms);

    return mailbox != NULL;
}

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_BYTE_WRITE_MODULE_I2C-0x10
union cms_op_byte_write_module_i2c {
    struct cms_mailbox overlay;
    struct {
        // Word 0
        struct cms_mailbox_header header;

        struct {
            // Word 1-3
            struct cms_op_module_i2c_select select;

            // Word 4
            struct {
                uint32_t offset    : 8;  // [7:0]
                uint32_t reserved0 : 24; // [31:8]
            };

            // Word 5
            struct {
                uint32_t value     : 8;  // [7:0]
                uint32_t reserved1 : 24; // [31:8]
            };
        } request;
    } message;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
bool cms_module_byte_write(struct cms* cms,
                           const struct cms_module_id* id,
                           uint8_t offset,
                           uint8_t value) {
    bool upper = offset >= CMS_MODULE_PAGE_SIZE;
    union cms_op_byte_write_module_i2c op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_BYTE_WRITE_MODULE_I2C,
            },
            .request = {
                .select = {
                    .cage = id->cage,
                    .page = upper ? id->page : 0, // Lower memory mapping is fixed.
                    .upper = upper ? 1 : 0, // Upper memory mapping is paged (except SFP).
                    .sfp_diag = id->sfp_diag ? 1 : 0,
                    .cmis_bank_valid = id->cmis ? 1 : 0,
                    .cmis_bank = id->cmis ? id->bank : 0,
                },
                .offset = offset,
                .value = value,
            },
        },
    };

    cms_lock(cms);
    volatile struct cms_mailbox* mailbox = cms_mailbox_post(
        cms->blk, &op.overlay, sizeof(op.message.request) / sizeof(uint32_t));
    cms_unlock(cms);

    return mailbox != NULL;
}

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_READ_MODULE_LOW_SPEED_IO-0x0D
union cms_op_read_module_low_speed_io {
    struct cms_mailbox overlay;
    struct {
        // Word 0
        struct cms_mailbox_header header;

        struct {
            // Word 1
            struct {
                uint32_t cage     : 2;  // [1:0]
                uint32_t reserved : 30; // [31:2]
            };
        } request;

        struct {
            // Word 2
            union {
                struct {
                    uint32_t reserved0 : 3;  // [2:0]
                    uint32_t prs       : 1;  // [3]
                    uint32_t reserved1 : 28; // [31:4]
                } sfp;
                struct {
                    uint32_t rst       : 1;  // [0]
                    uint32_t lpw       : 1;  // [1]
                    uint32_t reserved0 : 1;  // [2]
                    uint32_t prs       : 1;  // [3]
                    uint32_t int_      : 1;  // [4]
                    uint32_t reserved1 : 27; // [31:5]
                } dsfp;
                struct {
                    uint32_t reset_l   : 1;  // [0]
                    uint32_t lpmode    : 1;  // [1]
                    uint32_t modsel_l  : 1;  // [2]
                    uint32_t modprs_l  : 1;  // [3]
                    uint32_t int_l     : 1;  // [4]
                    uint32_t reserved0 : 27; // [31:5]
                } qsfp;
            };
        } response;
    } message;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
bool cms_module_gpio_read(struct cms* cms, uint8_t cage, struct cms_module_gpio* gpio) {
    union cms_op_read_module_low_speed_io op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_READ_MODULE_LOW_SPEED_IO,
            },
            .request = {
                .cage = cage,
            },
        },
    };

    bool valid = false;
    cms_lock(cms);

    volatile struct cms_mailbox* mailbox = cms_mailbox_post(
        cms->blk, &op.overlay, sizeof(op.message.request) / sizeof(uint32_t));
    if (mailbox == NULL) {
        goto unlock;
    }

    volatile union cms_op_read_module_low_speed_io* mb = (typeof(mb))mailbox;
    switch (gpio->type) {
    case cms_module_gpio_type_SFP:
        gpio->sfp.present = mb->message.response.sfp.prs != 0;
        valid = true;
        break;

    case cms_module_gpio_type_DSFP:
        gpio->dsfp.reset = mb->message.response.dsfp.rst != 0;
        gpio->dsfp.low_power_mode = mb->message.response.dsfp.lpw != 0;
        gpio->dsfp.present = mb->message.response.dsfp.prs != 0;
        gpio->dsfp.interrupt = mb->message.response.dsfp.int_ != 0;
        valid = true;
        break;

    case cms_module_gpio_type_QSFP:
        gpio->qsfp.reset = mb->message.response.qsfp.reset_l == 0;
        gpio->qsfp.low_power_mode = mb->message.response.qsfp.lpmode != 0;
        gpio->qsfp.select = mb->message.response.qsfp.modsel_l == 0;
        gpio->qsfp.present = mb->message.response.qsfp.modprs_l == 0;
        gpio->qsfp.interrupt = mb->message.response.qsfp.int_l == 0;
        valid = true;
        break;

    default:
        break;
    }

unlock:
    cms_unlock(cms);
    return valid;
}

//--------------------------------------------------------------------------------------------------
// https://docs.amd.com/r/en-US/pg348-cms-subsystem/CMS_OP_WRITE_MODULE_LOW_SPEED_IO-0x0E
union cms_op_write_module_low_speed_io {
    struct cms_mailbox overlay;
    struct {
        // Word 0
        struct cms_mailbox_header header;

        struct {
            // Word 1
            struct {
                uint32_t cage     : 2;  // [1:0]
                uint32_t reserved : 30; // [31:2]
            };

            // Word 2
            union {
                struct {
                    uint32_t rst      : 1;  // [0]
                    uint32_t lpw      : 1;  // [1]
                    uint32_t reserved : 30; // [31:2]
                } dsfp;
                struct {
                    uint32_t reset_l  : 1;  // [0]
                    uint32_t lpmode   : 1;  // [1]
                    uint32_t reserved : 30; // [31:2]
                } qsfp;
            };
        } request;
    } message;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
bool cms_module_gpio_write(struct cms* cms, uint8_t cage, const struct cms_module_gpio* gpio) {
    union cms_op_write_module_low_speed_io op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_WRITE_MODULE_LOW_SPEED_IO,
            },
            .request = {
                .cage = cage,
            },
        },
    };

    switch (gpio->type) {
    case cms_module_gpio_type_DSFP:
        op.message.request.dsfp.rst = gpio->dsfp.reset ? 1 : 0;
        op.message.request.dsfp.lpw = gpio->dsfp.low_power_mode ? 1 : 0;
        break;

    case cms_module_gpio_type_QSFP:
        op.message.request.qsfp.reset_l = gpio->qsfp.reset ? 0 : 1;
        op.message.request.qsfp.lpmode = gpio->qsfp.low_power_mode ? 1 : 0;
        break;

    default:
        return false;
    }

    cms_lock(cms);
    volatile struct cms_mailbox* mailbox = cms_mailbox_post(
        cms->blk, &op.overlay, sizeof(op.message.request) / sizeof(uint32_t));
    cms_unlock(cms);

    return mailbox != NULL;
}
