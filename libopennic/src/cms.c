#include "cms.h"

#include "array_size.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include "memory-barriers.h"
#include <netinet/ether.h>
#include "stats.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unused.h"

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
enum cms_msg_error {
    cms_msg_error_NONE,
    cms_msg_error_BAD_OPCODE_ERR,
    cms_msg_error_BRD_INFO_MISSING_ERR,
    cms_msg_error_LENGTH_ERR,
    cms_msg_error_SAT_FW_WRITE_FAIL,
    cms_msg_error_SAT_FW_UPDATE_FAIL,
    cms_msg_error_SAT_FW_LOAD_FAIL,
    cms_msg_error_SAT_FW_ERASE_FAIL,
    cms_msg_error_RESERVED0,
    cms_msg_error_CSDR_FAILED,
    cms_msg_error_QSFP_FAIL,
};

static const char* cms_msg_error_to_str(enum cms_msg_error error) {
    const char* str = "UNKNOWN";
    switch (error) {
#define CASE_MSG_ERROR(_name) case cms_msg_error_##_name: str = #_name; break
    CASE_MSG_ERROR(NONE);
    CASE_MSG_ERROR(BAD_OPCODE_ERR);
    CASE_MSG_ERROR(BRD_INFO_MISSING_ERR);
    CASE_MSG_ERROR(LENGTH_ERR);
    CASE_MSG_ERROR(SAT_FW_WRITE_FAIL);
    CASE_MSG_ERROR(SAT_FW_UPDATE_FAIL);
    CASE_MSG_ERROR(SAT_FW_LOAD_FAIL);
    CASE_MSG_ERROR(SAT_FW_ERASE_FAIL);
    CASE_MSG_ERROR(RESERVED0);
    CASE_MSG_ERROR(CSDR_FAILED);
    CASE_MSG_ERROR(QSFP_FAIL);
#undef CASE_MSG_ERROR
    }

    return str;
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
#define MAX_ATTEMPTS 5

    volatile struct cms_mailbox* mailbox = NULL;
    for (unsigned int attempt = 1; mailbox == NULL && attempt <= MAX_ATTEMPTS; ++attempt) {
        if (!cms_mailbox_is_ready(cms)) {
            log_err(EBUSY, "mailbox ready timeout [attempt=%u/%u]", attempt, MAX_ATTEMPTS);
            break;
        }

        cms_error_clear(cms);
        if (!cms_error_is_cleared(cms)) {
            log_err(EBUSY, "error clear timeout [attempt=%u/%u]", attempt, MAX_ATTEMPTS);
            break;
        }

        mailbox = cms_mailbox_get(cms);
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
            log_err(EBUSY, "mailbox done timeout [attempt=%u/%u]", attempt, MAX_ATTEMPTS);
            mailbox = NULL;
            // Fall through to capture error codes.
        }

        union cms_error_reg error = {._v = cms->error_reg._v};
        if (error.pkt_error) {
            enum cms_msg_error err = cms->host_msg_error_reg;
            log_err(EIO, "packet error %u (%s) [attempt=%u/%u]",
                    err, cms_msg_error_to_str(err), attempt, MAX_ATTEMPTS);
            mailbox = NULL;
            // Fall through to capture error codes.
        }

        if (error.sat_ctrl_err) {
            log_err(EIO, "satellite controller error %u (%s) [attempt=%u/%u]",
                    error.sat_ctrl_err_code, cms_sc_error_to_str(error.sat_ctrl_err_code),
                    attempt, MAX_ATTEMPTS);
            mailbox = NULL;
        }
    }

    return mailbox;

#undef MAX_ATTEMPTS
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

        info->fan_presence = field->value.u8;
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
        log_err(ENOMEM, "failed to alloc card info");
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
        log_err(EIO, "failed to query card info");
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
        log_err(EIO, "failed to read %s page %u of cage %u",
                id->upper ? "upper" : "lower", id->page, id->cage);
        goto unlock;
    }

    volatile union cms_op_block_read_module_i2c* mb = (typeof(mb))mailbox;
    size_t size = mb->message.response.size;
    if (size != sizeof(page->u8)) {
        log_err(EIO, "read of %s page %u of cage %u responded with %zu bytes, expected %zu",
                id->upper ? "upper" : "lower", id->page, id->cage, size, sizeof(page->u8));
        goto unlock;
    }

    page = malloc(sizeof(*page));
    if (page == NULL) {
        log_err(ENOMEM, "failed to alloc %zu bytes for %s page %u of cage %u",
                sizeof(*page), id->upper ? "upper" : "lower", id->page, id->cage);
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
    uint8_t page = upper ? id->page : 0;
    union cms_op_byte_read_module_i2c op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_BYTE_READ_MODULE_I2C,
            },
            .request = {
                .select = {
                    .cage = id->cage,
                    .page = page, // Lower memory mapping is fixed.
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
    } else {
        log_err(EIO, "failed to read byte at offset 0x%02x from %s page %u of cage %u",
                offset, upper ? "upper" : "lower", page, id->cage);
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
    uint8_t page = upper ? id->page : 0;
    union cms_op_byte_write_module_i2c op = {
        .message = {
            .header = {
                .opcode = cms_mailbox_opcode_BYTE_WRITE_MODULE_I2C,
            },
            .request = {
                .select = {
                    .cage = id->cage,
                    .page = page, // Lower memory mapping is fixed.
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

    if (mailbox == NULL) {
        log_err(EIO, "failed to write byte at offset 0x%02x to %s page %u of cage %u",
                offset, upper ? "upper" : "lower", page, id->cage);
        return false;
    }
    return true;
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
        log_err(EIO, "failed to read gpio for cage %u", cage);
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

    if (mailbox == NULL) {
        log_err(EIO, "failed to write gpio for cage %u", cage);
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
union cms_card_metric_flags {
    struct {
        uint64_t is_adc   : 1; // [0]
        uint64_t in_milli : 1; // [1]
    };
    uint64_t _v;
} __attribute__((packed));

struct cms_card_metric {
    struct stats_metric_spec spec;
    union cms_card_metric_flags flags;
    uint64_t profile_mask;
};

#define _CMS_CARD_METRIC_ADC(_name, _in_milli, _profile_mask) \
{ \
    .spec = STATS_METRIC_SPEC_IO( \
        #_name, NULL, GAUGE, 0, 0, \
        struct cms_block, _name##_reg, 0, 0, false, STATS_IO_DATA_NULL \
     ), \
    .flags = { \
        .is_adc = 1, \
        .in_milli = (_in_milli) ? 1 : 0, \
    }, \
    .profile_mask = _profile_mask, \
}

#define CMS_CARD_METRIC_ADC(_name, _in_milli, _profile_mask) \
    _CMS_CARD_METRIC_ADC(_name##_max, _in_milli, _profile_mask), \
    _CMS_CARD_METRIC_ADC(_name##_avg, _in_milli, _profile_mask), \
    _CMS_CARD_METRIC_ADC(_name##_ins, _in_milli, _profile_mask)

#define CMS_CARD_METRIC_BIT(_name, _pos, _invert, _profile_mask) \
{ \
    .spec = STATS_METRIC_SPEC_IO( \
        #_name, NULL, FLAG, 0, 0, \
        struct cms_block, _name##_ins_reg, 1, _pos, _invert, STATS_IO_DATA_NULL \
     ), \
    .profile_mask = _profile_mask, \
}

// TODO: add units as extra label for each metric?
static const struct cms_card_metric cms_card_metrics[] = {
#define PM(_name) CMS_PROFILE_MASK(_name)
    // 10-bit ADC formatted metrics
    //                  name            in_milli      profile_mask
    CMS_CARD_METRIC_ADC(_1v2_vccio,         true,     0                 ),
    CMS_CARD_METRIC_ADC(_2v5_vpp23,         true,     0                 ),
    CMS_CARD_METRIC_ADC(_3v3_aux,           true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(_3v3_pex,           true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(_3v3pex_i_in,       true,                PM(U55)),
    CMS_CARD_METRIC_ADC(_12v_aux,           true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(_12v_aux1,          true,     0                 ),
    CMS_CARD_METRIC_ADC(_12v_aux_i_in,      true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(_12v_pex,           true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(_12v_sw,            true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(_12vpex_i_in,       true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(aux_3v3_i,          true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(cage_temp0,         false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(cage_temp1,         false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(cage_temp2,         false,    0                 ),
    CMS_CARD_METRIC_ADC(cage_temp3,         false,    0                 ),
    CMS_CARD_METRIC_ADC(ddr4_vpp_btm,       true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(ddr4_vpp_top,       true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(dimm_temp0,         false,    PM(U280)          ),
    CMS_CARD_METRIC_ADC(dimm_temp1,         false,    PM(U280)          ),
    CMS_CARD_METRIC_ADC(dimm_temp2,         false,    0                 ),
    CMS_CARD_METRIC_ADC(dimm_temp3,         false,    0                 ),
    CMS_CARD_METRIC_ADC(fan_speed,          false,    PM(U280)          ),
    CMS_CARD_METRIC_ADC(fan_temp,           false,    PM(U280)          ),
    CMS_CARD_METRIC_ADC(fpga_temp,          false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(gtavcc,             true,     0                 ),
    CMS_CARD_METRIC_ADC(gtvcc_aux,          true,     0                 ),
    CMS_CARD_METRIC_ADC(hbm_1v2,            true,                PM(U55)),
    CMS_CARD_METRIC_ADC(hbm_1v2_i,          true,                PM(U55)),
    CMS_CARD_METRIC_ADC(hbm_temp1,          false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(hbm_temp2,          false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(mgt0v9avcc,         true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(cmc_mgtavcc,        true,     0                 ),
    CMS_CARD_METRIC_ADC(cmc_mgtavcc_i,      true,     0                 ),
    CMS_CARD_METRIC_ADC(mgtavtt,            true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(cmc_mgtavtt_i,      true,     0                 ),
    CMS_CARD_METRIC_ADC(pex_3v3_power,      true,     0                 ),
    CMS_CARD_METRIC_ADC(pex_12v_power,      true,     0                 ),
    CMS_CARD_METRIC_ADC(se98_temp0,         false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(se98_temp1,         false,    PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(se98_temp2,         false,    0                 ),
    CMS_CARD_METRIC_ADC(sys_5v5,            true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(v12_in_aux0_i,      true,     0                 ),
    CMS_CARD_METRIC_ADC(v12_in_aux1_i,      true,     0                 ),
    CMS_CARD_METRIC_ADC(v12_in_i,           true,     0                 ),
    CMS_CARD_METRIC_ADC(vcc0v85,            true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(vcc1v2_btm,         true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(vcc1v2_i,           true,     0                 ),
    CMS_CARD_METRIC_ADC(vcc1v2_top,         true,     PM(U280)          ),
    CMS_CARD_METRIC_ADC(cmc_vcc1v5,         true,     0                 ),
    CMS_CARD_METRIC_ADC(vcc1v8,             true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(vcc3v3,             true,                PM(U55)),
    CMS_CARD_METRIC_ADC(vcc_5v0,            true,     0                 ),
    CMS_CARD_METRIC_ADC(vccaux,             true,     0                 ),
    CMS_CARD_METRIC_ADC(vccaux_pmc,         true,     0                 ),
    CMS_CARD_METRIC_ADC(vccint,             true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(vccint_i,           true,     PM(U280) | PM(U55)),
    CMS_CARD_METRIC_ADC(vccint_io,          true,                PM(U55)),
    CMS_CARD_METRIC_ADC(vccint_io_i,        true,                PM(U55)),
    CMS_CARD_METRIC_ADC(vccint_power,       true,     0                 ),
    CMS_CARD_METRIC_ADC(vccint_temp,        false,               PM(U55)),
    CMS_CARD_METRIC_ADC(vccint_vcu_0v9,     true,     0                 ),
    CMS_CARD_METRIC_ADC(vccram,             true,     0                 ),
    CMS_CARD_METRIC_ADC(vccsoc,             true,     0                 ),
    CMS_CARD_METRIC_ADC(vpp2v5,             true,                PM(U55)),

    // Flag metrics
    //                  name         pos    invert    profile_mask
    CMS_CARD_METRIC_BIT(power_good,    0,    true,    PM(U280) | PM(U55)),
#undef PM
};

//--------------------------------------------------------------------------------------------------
static void cms_card_stats_attach_metrics(const struct stats_block_spec* bspec) {
    volatile struct cms_block* cms = bspec->io.base;

    union cms_control_reg control = {._v = cms->control_reg._v};
    control.hbm_temp_monitor_enable = 1; // Enable temperature monitoring on high-bandwidth memory.
    cms->control_reg._v = control._v;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static double cms_card_stats_convert_metric(const struct stats_block_spec* UNUSED(bspec),
                                            const struct stats_metric_spec* mspec,
                                            uint64_t value,
                                            void* UNUSED(data)) {
    union cms_card_metric_flags flags = {._v = mspec->io.data.u64};

    if (flags.is_adc && flags.in_milli) {
        return (double)value / 1000.0;
    }

    return (double)value;
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* cms_card_stats_zone_alloc(struct stats_domain* domain,
                                             struct cms* cms,
                                             const char* name) {
    if (!cms_reg_map_is_ready(cms->blk)) {
        log_err(EBUSY, "regmap ready timeout");
        return NULL;
    }

    enum cms_profile profile = cms_profile_get(cms->blk);
    uint64_t profile_mask = _CMS_PROFILE_MASK(profile);

    unsigned int nmetrics = 0;
    const struct cms_card_metric* cm;
    for (cm = cms_card_metrics; cm < &cms_card_metrics[ARRAY_SIZE(cms_card_metrics)]; ++cm) {
        if ((cm->profile_mask & profile_mask) != 0) {
            nmetrics += 1;
        }
    }

    struct stats_metric_spec mspecs[nmetrics];
    struct stats_metric_spec* mspec = mspecs;
    for (cm = cms_card_metrics; cm < &cms_card_metrics[ARRAY_SIZE(cms_card_metrics)]; ++cm) {
        if ((cm->profile_mask & profile_mask) != 0) {
            *mspec = cm->spec;
            mspec->io.data.u64 = cm->flags._v;
            mspec += 1;
        }
    }

    struct stats_block_spec bspecs[] = {
      {
          .name = "cms",
          .metrics = mspecs,
          .nmetrics = nmetrics,
          .io = {
              .base = cms->blk,
          },
          .attach_metrics = cms_card_stats_attach_metrics,
          .convert_metric = cms_card_stats_convert_metric,
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
void cms_card_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}

//--------------------------------------------------------------------------------------------------
enum cms_module_metric_type { // Limited to 1 byte (see cms_module_metric_flags below).
    cms_module_metric_type_GPIO,
    cms_module_metric_type_ALARM,
    cms_module_metric_type_MONITOR_MIN,
    cms_module_metric_type_MONITOR_TEMP = cms_module_metric_type_MONITOR_MIN,
    cms_module_metric_type_MONITOR_VOLTAGE,
    cms_module_metric_type_MONITOR_CURRENT,
    cms_module_metric_type_MONITOR_OPTICAL_POWER,
    cms_module_metric_type_MONITOR_MAX,
};

union cms_module_metric_flags {
    struct {
        uint64_t type : 8; // [7:0] (enum cms_module_metric_type)
    };
    uint64_t _v;
} __attribute__((packed));

struct cms_module_metric {
    struct stats_metric_spec spec;
    union cms_module_metric_flags flags;
};

#define CMS_MODULE_METRIC_GPIO(_type, _name) \
{ \
    .spec = { \
        __STATS_METRIC_SPEC(#_type "_gpio_" #_name, NULL, FLAG, 0, 0), \
        .io = { \
            .offset = offsetof(struct cms_module_gpio, _type._name), \
        }, \
    }, \
    .flags = { \
        .type = cms_module_metric_type_GPIO, \
    }, \
}

#define CMS_MODULE_METRIC_QSFP_GPIO(_name) CMS_MODULE_METRIC_GPIO(qsfp, _name)

#define CMS_MODULE_METRIC_ALARM(_name, _offset, _pos) \
{ \
    .spec = { \
        __STATS_METRIC_SPEC(#_name, NULL, FLAG, 0, 0), \
        .io = { \
            .offset = _offset, \
            .size = sizeof(uint8_t), \
            .width = 1, \
            .shift = _pos, \
        }, \
    }, \
    .flags = { \
        .type = cms_module_metric_type_ALARM, \
    }, \
}

#define CMS_MODULE_METRIC_MONITOR(_name, _type, _offset) \
{ \
    .spec = { \
        __STATS_METRIC_SPEC(#_name, NULL, GAUGE, 0, 0), \
        .io = { \
            .offset = _offset, \
            .size = sizeof(union sff_8636_u16), \
        }, \
    }, \
    .flags = { \
        .type = cms_module_metric_type_MONITOR_##_type, \
    }, \
}

// TODO: add units as extra label for each metric?
static const struct cms_module_metric cms_module_metrics[] = {
    CMS_MODULE_METRIC_QSFP_GPIO(reset),
    CMS_MODULE_METRIC_QSFP_GPIO(low_power_mode),
    CMS_MODULE_METRIC_QSFP_GPIO(select),
    CMS_MODULE_METRIC_QSFP_GPIO(present),
    CMS_MODULE_METRIC_QSFP_GPIO(interrupt),

    CMS_MODULE_METRIC_ALARM(channel_los_rx1, 3, 0),
    CMS_MODULE_METRIC_ALARM(channel_los_rx2, 3, 1),
    CMS_MODULE_METRIC_ALARM(channel_los_rx3, 3, 2),
    CMS_MODULE_METRIC_ALARM(channel_los_rx4, 3, 3),
    CMS_MODULE_METRIC_ALARM(channel_los_tx1, 3, 4),
    CMS_MODULE_METRIC_ALARM(channel_los_tx2, 3, 5),
    CMS_MODULE_METRIC_ALARM(channel_los_tx3, 3, 6),
    CMS_MODULE_METRIC_ALARM(channel_los_tx4, 3, 7),

    CMS_MODULE_METRIC_ALARM(channel_fault_tx1,           4, 0),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx2,           4, 1),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx3,           4, 2),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx4,           4, 3),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx1_equalizer, 4, 4),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx2_equalizer, 4, 5),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx3_equalizer, 4, 6),
    CMS_MODULE_METRIC_ALARM(channel_fault_tx4_equalizer, 4, 7),

    CMS_MODULE_METRIC_ALARM(channel_lol_rx1, 5, 0),
    CMS_MODULE_METRIC_ALARM(channel_lol_rx2, 5, 1),
    CMS_MODULE_METRIC_ALARM(channel_lol_rx3, 5, 2),
    CMS_MODULE_METRIC_ALARM(channel_lol_rx4, 5, 3),
    CMS_MODULE_METRIC_ALARM(channel_lol_tx1, 5, 4),
    CMS_MODULE_METRIC_ALARM(channel_lol_tx2, 5, 5),
    CMS_MODULE_METRIC_ALARM(channel_lol_tx3, 5, 6),
    CMS_MODULE_METRIC_ALARM(channel_lol_tx4, 5, 7),

    CMS_MODULE_METRIC_ALARM(temp_low_warning,  6, 4),
    CMS_MODULE_METRIC_ALARM(temp_high_warning, 6, 5),
    CMS_MODULE_METRIC_ALARM(temp_low_alarm,    6, 6),
    CMS_MODULE_METRIC_ALARM(temp_high_alarm,   6, 7),

    CMS_MODULE_METRIC_ALARM(vcc_low_warning,  7, 4),
    CMS_MODULE_METRIC_ALARM(vcc_high_warning, 7, 5),
    CMS_MODULE_METRIC_ALARM(vcc_low_alarm,    7, 6),
    CMS_MODULE_METRIC_ALARM(vcc_high_alarm,   7, 7),

    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_rx2,   9, 0),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_rx2,  9, 1),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_rx2,     9, 2),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_rx2,    9, 3),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_rx1,   9, 4),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_rx1,  9, 5),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_rx1,     9, 6),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_rx1,    9, 7),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_rx4,  10, 0),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_rx4, 10, 1),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_rx4,    10, 2),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_rx4,   10, 3),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_rx3,  10, 4),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_rx3, 10, 5),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_rx3,    10, 6),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_rx3,   10, 7),

    CMS_MODULE_METRIC_ALARM(channel_bias_low_warning_tx2,  11, 0),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_warning_tx2, 11, 1),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_alarm_tx2,    11, 2),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_alarm_tx2,   11, 3),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_warning_tx1,  11, 4),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_warning_tx1, 11, 5),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_alarm_tx1,    11, 6),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_alarm_tx1,   11, 7),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_warning_tx4,  12, 0),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_warning_tx4, 12, 1),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_alarm_tx4,    12, 2),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_alarm_tx4,   12, 3),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_warning_tx3,  12, 4),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_warning_tx3, 12, 5),
    CMS_MODULE_METRIC_ALARM(channel_bias_low_alarm_tx3,    12, 6),
    CMS_MODULE_METRIC_ALARM(channel_bias_high_alarm_tx3,   12, 7),

    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_tx2,  13, 0),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_tx2, 13, 1),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_tx2,    13, 2),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_tx2,   13, 3),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_tx1,  13, 4),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_tx1, 13, 5),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_tx1,    13, 6),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_tx1,   13, 7),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_tx4,  14, 0),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_tx4, 14, 1),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_tx4,    14, 2),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_tx4,   14, 3),
    CMS_MODULE_METRIC_ALARM(channel_power_low_warning_tx3,  14, 4),
    CMS_MODULE_METRIC_ALARM(channel_power_high_warning_tx3, 14, 5),
    CMS_MODULE_METRIC_ALARM(channel_power_low_alarm_tx3,    14, 6),
    CMS_MODULE_METRIC_ALARM(channel_power_high_alarm_tx3,   14, 7),

    CMS_MODULE_METRIC_MONITOR(temp, TEMP,    22),
    CMS_MODULE_METRIC_MONITOR(vcc,  VOLTAGE, 26),

    CMS_MODULE_METRIC_MONITOR(channel_power_rx1, OPTICAL_POWER, 34),
    CMS_MODULE_METRIC_MONITOR(channel_power_rx2, OPTICAL_POWER, 36),
    CMS_MODULE_METRIC_MONITOR(channel_power_rx3, OPTICAL_POWER, 38),
    CMS_MODULE_METRIC_MONITOR(channel_power_rx4, OPTICAL_POWER, 40),

    CMS_MODULE_METRIC_MONITOR(channel_bias_tx1, CURRENT, 42),
    CMS_MODULE_METRIC_MONITOR(channel_bias_tx2, CURRENT, 44),
    CMS_MODULE_METRIC_MONITOR(channel_bias_tx3, CURRENT, 46),
    CMS_MODULE_METRIC_MONITOR(channel_bias_tx4, CURRENT, 48),

    CMS_MODULE_METRIC_MONITOR(channel_power_tx1, OPTICAL_POWER, 50),
    CMS_MODULE_METRIC_MONITOR(channel_power_tx2, OPTICAL_POWER, 52),
    CMS_MODULE_METRIC_MONITOR(channel_power_tx3, OPTICAL_POWER, 54),
    CMS_MODULE_METRIC_MONITOR(channel_power_tx4, OPTICAL_POWER, 56),
};

//--------------------------------------------------------------------------------------------------
union cms_module_block_io_data {
    struct {
        uint64_t cage : 2; // [1:0]
    };
    uint64_t _v;
} __attribute__((packed));

struct cms_module_block_latch_data {
    struct cms_module_gpio gpio;
    union cms_module_page* lo;
};

//--------------------------------------------------------------------------------------------------
static void cms_module_stats_latch_metrics(const struct stats_block_spec* bspec, void* data) {
    struct cms* cms = (typeof(cms))bspec->io.base;
    union cms_module_block_io_data io = {._v = bspec->io.data.u64};
    struct cms_module_block_latch_data* latch = data;

    latch->gpio.type = cms_module_gpio_type_QSFP;
    if (cms_module_gpio_read(cms, io.cage, &latch->gpio) &&
        latch->gpio.qsfp.present && !latch->gpio.qsfp.reset) {
        struct cms_module_id id = {.cage = io.cage};
        latch->lo = cms_module_page_read(cms, &id);
    }
}

//--------------------------------------------------------------------------------------------------
static void cms_module_stats_release_metrics(const struct stats_block_spec* UNUSED(bspec),
                                             void* data) {
    struct cms_module_block_latch_data* latch = data;
    if (latch->lo != NULL) {
        cms_module_page_free(latch->lo);
    }
}

//--------------------------------------------------------------------------------------------------
static void cms_module_stats_read_metric(const struct stats_block_spec* UNUSED(bspec),
                                         const struct stats_metric_spec* mspec,
                                         uint64_t* value,
                                         void* data) {
    struct cms_module_block_latch_data* latch = data;
    bool have_lo = latch->lo != NULL;

    union cms_module_metric_flags flags = {._v = mspec->io.data.u64};
    *value = 0;
    switch (flags.type) {
    case cms_module_metric_type_GPIO:
        *value = *((bool*)(((void*)&latch->gpio) + mspec->io.offset)) ? 1 : 0;
        break;

    case cms_module_metric_type_ALARM:
        if (have_lo) {
            *value = (latch->lo->u8[mspec->io.offset] >> mspec->io.shift) & 1;
        }
        break;

    case cms_module_metric_type_MONITOR_MIN ... cms_module_metric_type_MONITOR_MAX - 1:
        if (have_lo) {
            union sff_8636_u16* u16 = (typeof(u16))&latch->lo->u8[mspec->io.offset];
            *value = (u16->msb << 8) | u16->lsb;
        } else {
            *value = UINT64_MAX;
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
static double cms_module_stats_convert_metric(const struct stats_block_spec* UNUSED(bspec),
                                              const struct stats_metric_spec* mspec,
                                              uint64_t value,
                                              void* UNUSED(data)) {
    if (value == UINT64_MAX) {
        return HUGE_VAL;
    }

    union cms_module_metric_flags flags = {._v = mspec->io.data.u64};
    double f64 = 0.0;
    switch (flags.type) {
    case cms_module_metric_type_GPIO:
    case cms_module_metric_type_ALARM:
        f64 = value != 0 ? 1.0 : 0.0;
        break;

    case cms_module_metric_type_MONITOR_TEMP:
        f64 = (double)((int16_t)value) / 256.0; // degrees C
        break;

    case cms_module_metric_type_MONITOR_VOLTAGE:
        f64 = (double)((uint16_t)value) * 100e-6; // Volts
        break;

    case cms_module_metric_type_MONITOR_CURRENT:
        f64 = (double)((uint16_t)value) * 2e-6; // Amps
        break;

    case cms_module_metric_type_MONITOR_OPTICAL_POWER:
        f64 = (double)((uint16_t)value) * 0.1e-6; // Watts
        break;
    }

    return f64;
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* cms_module_stats_zone_alloc(struct stats_domain* domain,
                                               struct cms* cms,
                                               uint8_t cage,
                                               const char* name) {
    if (!cms_reg_map_is_ready(cms->blk)) {
        log_err(EBUSY, "regmap ready timeout");
        return NULL;
    }

    struct stats_metric_spec mspecs[ARRAY_SIZE(cms_module_metrics)];
    struct stats_metric_spec* mspec = mspecs;
    const struct cms_module_metric* mm;
    for (mm = cms_module_metrics; mm < &cms_module_metrics[ARRAY_SIZE(cms_module_metrics)]; ++mm) {
        *mspec = mm->spec;
        mspec->io.data.u64 = mm->flags._v;
        mspec += 1;
    }

    union cms_module_block_io_data data = {
        .cage = cage,
    };
    struct stats_block_spec bspecs[] = {
      {
          .name = "cms",
          .metrics = mspecs,
          .nmetrics = ARRAY_SIZE(mspecs),
          .io = {
              .base = cms,
              .data.u64 = data._v,
          },
          .latch = {
              .data_size = sizeof(struct cms_module_block_latch_data),
          },
          .latch_metrics = cms_module_stats_latch_metrics,
          .release_metrics = cms_module_stats_release_metrics,
          .read_metric = cms_module_stats_read_metric,
          .convert_metric = cms_module_stats_convert_metric,
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
void cms_module_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}

//--------------------------------------------------------------------------------------------------
bool cms_module_stats_is_gpio_metric(const struct stats_metric_spec* mspec) {
    union cms_module_metric_flags flags = {._v = mspec->io.data.u64};
    return flags.type == cms_module_metric_type_GPIO;
}

bool cms_module_stats_is_alarm_metric(const struct stats_metric_spec* mspec) {
    union cms_module_metric_flags flags = {._v = mspec->io.data.u64};
    return flags.type == cms_module_metric_type_ALARM;
}

bool cms_module_stats_is_monitor_metric(const struct stats_metric_spec* mspec) {
    union cms_module_metric_flags flags = {._v = mspec->io.data.u64};
    return flags.type >= cms_module_metric_type_MONITOR_MIN &&
           flags.type < cms_module_metric_type_MONITOR_MAX;
}
