#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>
#include <sstream>
#include <stdint.h>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

#include "cms.h"
#include "esnet_smartnic_toplevel.h"
#include "sff-8024.h"
#include "sff-8636.h"

using namespace grpc;
using namespace sn_cfg::v1;
using namespace std;

//--------------------------------------------------------------------------------------------------
class ModulePages {
public:
    ModulePages(struct cms* cms, unsigned int mod_id) :
        _error(false),
        _lo(NULL),
        _u00(NULL),
        _u02(NULL) {
        struct cms_module_id id = {
            .cage = (uint8_t)mod_id,
            .page = 0,
            .upper = false,
            .cmis = false,
            .bank = 0,
            .sfp_diag = false,
        };

        _lo = cms_module_page_read(cms, &id);
        if (_lo == NULL) {
            _error = true;
            return;
        }
        auto lo = lower();

        id.upper = true;
        _u00 = cms_module_page_read(cms, &id);
        if (_u00 == NULL) {
            _error = true;
            return;
        }
        auto u00 = upper_00();

        // Read optional upper pages if available.
        if (!lo->status.flags.flat_mem) {
            if (u00->options.device.page_02) {
                id.page = 2;
                _u02 = cms_module_page_read(cms, &id);
                if (_u02 == NULL) {
                    _error = true;
                    return;
                }
            }
        }
    }

    ~ModulePages() {
        if (_lo != NULL) {
            cms_module_page_free(_lo);
        }

        if (_u00 != NULL) {
            cms_module_page_free(_u00);
        }

        if (_u02 != NULL) {
            cms_module_page_free(_u02);
        }
    }

    bool error() const {return _error;}

    const struct sff_8636_lower_page* lower() const {
        return _lo == NULL ? NULL : &_lo->sff_8636.lower.page;
    }

    const struct sff_8636_upper_page_00* upper_00() const {
        return _u00 == NULL ? NULL : &_u00->sff_8636.upper.page_00;
    }

    const struct sff_8636_upper_page_02* upper_02() const {
        return _u02 == NULL ? NULL : &_u02->sff_8636.upper.page_02;
    }

private:
    bool _error;

    union cms_module_page* _lo;
    union cms_module_page* _u00;
    union cms_module_page* _u02;
};

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_module(Device* dev) {
    for (unsigned int mod_id = 0; mod_id < dev->nports; ++mod_id) {
        ostringstream name;
        name << "module" << mod_id;

        auto stats = new DeviceStats;
        stats->name = name.str();
        stats->zone = cms_module_stats_zone_alloc(
            dev->stats.domains[DeviceStatsDomain::MODULES], &dev->cms, mod_id, stats->name.c_str());
        if (stats->zone == NULL) {
            cerr << "ERROR: Failed to alloc stats zone for module ID " << mod_id << "."  << endl;
            exit(EXIT_FAILURE);
        }

        dev->stats.zones[DeviceStatsZone::MODULE_MONITORS].push_back(stats);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_module(Device* dev) {
    auto zones = &dev->stats.zones[DeviceStatsZone::MODULE_MONITORS];
    while (!zones->empty()) {
        auto stats = zones->back();
        cms_module_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }
}

//--------------------------------------------------------------------------------------------------
static inline ModuleGpioState bool_to_gpio_state(bool assert) {
    return assert ? ModuleGpioState::GPIO_STATE_ASSERT : ModuleGpioState::GPIO_STATE_DEASSERT;
}

static inline void bool_if_has_gpio_state(bool* assert, ModuleGpioState state) {
    if (state != ModuleGpioState::GPIO_STATE_UNKNOWN) {
        *assert = state == ModuleGpioState::GPIO_STATE_ASSERT;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_module_gpio(
    const ModuleGpioRequest& req,
    function<void(const ModuleGpioResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleGpioResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleGpioResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        auto rgpio = req.gpio();
        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleGpioResponse resp;
            auto err = ErrorCode::EC_OK;
            struct cms_module_gpio cms_gpio;

            // TODO: Query module type from card info?
            cms_gpio.type = cms_module_gpio_type_QSFP;
            if (!cms_module_gpio_read(&dev->cms, mod_id, &cms_gpio)) {
                err = ErrorCode::EC_MODULE_GPIO_READ_FAILED;
                goto write_error;
            }

            {
                auto gpio = resp.mutable_gpio();
                gpio->set_reset(bool_to_gpio_state(cms_gpio.qsfp.reset));
                gpio->set_low_power_mode(bool_to_gpio_state(cms_gpio.qsfp.low_power_mode));
                gpio->set_select(bool_to_gpio_state(cms_gpio.qsfp.select));
                gpio->set_present(bool_to_gpio_state(cms_gpio.qsfp.present));
                gpio->set_interrupt(bool_to_gpio_state(cms_gpio.qsfp.interrupt));
            }

        write_error:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_module_gpio(
    const ModuleGpioRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_module_gpio(req, [&rdwr](const ModuleGpioResponse& resp) -> void {
        BatchResponse bresp;
        auto gpio = bresp.mutable_module_gpio();
        gpio->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetModuleGpio(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleGpioRequest* req,
    ServerWriter<ModuleGpioResponse>* writer) {
    get_module_gpio(*req, [&writer](const ModuleGpioResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_module_gpio(
    const ModuleGpioRequest& req,
    function<void(const ModuleGpioResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleGpioResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleGpioResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        auto rgpio = req.gpio();
        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleGpioResponse resp;
            auto err = ErrorCode::EC_OK;
            struct cms_module_gpio cms_gpio;

            // TODO: Query module type from card info?
            cms_gpio.type = cms_module_gpio_type_QSFP;
            if (!cms_module_gpio_read(&dev->cms, mod_id, &cms_gpio)) {
                err = ErrorCode::EC_MODULE_GPIO_READ_FAILED;
                goto write_error;
            }

            bool_if_has_gpio_state(&cms_gpio.qsfp.reset, rgpio.reset());
            bool_if_has_gpio_state(&cms_gpio.qsfp.low_power_mode, rgpio.low_power_mode());

            if (!cms_module_gpio_write(&dev->cms, mod_id, &cms_gpio)) {
                err = ErrorCode::EC_MODULE_GPIO_WRITE_FAILED;
                goto write_error;
            }

        write_error:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_module_gpio(
    const ModuleGpioRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_module_gpio(req, [&rdwr](const ModuleGpioResponse& resp) -> void {
        BatchResponse bresp;
        auto gpio = bresp.mutable_module_gpio();
        gpio->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetModuleGpio(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleGpioRequest* req,
    ServerWriter<ModuleGpioResponse>* writer) {
    set_module_gpio(*req, [&writer](const ModuleGpioResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
static size_t count_printable(const char* ptr, size_t size) {
    if (size == 0) {
        return 0;
    }

    for (ssize_t sz = size - 1; sz >= 0; --sz) {
        char c = ptr[sz];
        if (isprint(c) && !isblank(c)) {
            return sz + 1;
        }
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
static void add_module_info_vendor(ModuleInfo& info, ModulePages& pages) {
    auto u00 = pages.upper_00();
    auto u02 = pages.upper_02();
    auto vendor = info.mutable_vendor();

    auto size = count_printable(u00->vendor_name, sizeof(u00->vendor_name));
    if (size > 0) {
        vendor->set_name(u00->vendor_name, size);
    } else {
        vendor->set_name("unspecified");
    }

    size = count_printable(u00->vendor_rev, sizeof(u00->vendor_rev));
    if (size > 0) {
        vendor->set_revision(u00->vendor_rev, size);
    } else {
        vendor->set_revision("unspecified");
    }

    size = count_printable(u00->vendor_pn, sizeof(u00->vendor_pn));
    if (size > 0) {
        vendor->set_part_number(u00->vendor_pn, size);
    } else {
        vendor->set_part_number("unspecified");
    }

    size = count_printable(u00->vendor_sn, sizeof(u00->vendor_sn));
    if (size > 0) {
        vendor->set_serial_number(u00->vendor_sn, size);
    } else {
        vendor->set_serial_number("unspecified");
    }

    const uint8_t* bytes = u00->vendor_oui;
    if (bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0) {
        vendor->set_oui("unspecified");
    } else {
        char tmp_str[8 + 1];
        snprintf(tmp_str, sizeof(tmp_str), "%02X:%02X:%02X", bytes[0], bytes[1], bytes[2]);
        vendor->set_oui(tmp_str);
    }

    if (u02 != NULL && u00->extended_identifier.clei_present) {
        vendor->set_clei(u02->clei, sizeof(u02->clei));
    } else {
        vendor->set_clei("unspecified");
    }

    auto date = vendor->mutable_date();
    date->set_year(u00->date_code.year, sizeof(u00->date_code.year));
    date->set_month(u00->date_code.month, sizeof(u00->date_code.month));
    date->set_day(u00->date_code.day, sizeof(u00->date_code.day));

    size = count_printable(u00->date_code.vendor, sizeof(u00->date_code.vendor));
    if (size > 0) {
        date->set_vendor(u00->date_code.vendor, size);
    } else {
        date->set_vendor("unspecified");
    }
}

//--------------------------------------------------------------------------------------------------
static void add_module_info_device(ModuleInfo& info, ModulePages& pages) {
    auto lo = pages.lower();
    auto u00 = pages.upper_00();
    auto dev = info.mutable_device();
    auto ident = dev->mutable_identifier();

    ident->set_identifier(sff_8024_identifier_to_str((enum sff_8024_identifier)u00->identifier));
    ident->set_revision_compliance(sff_8636_compliance_revision_to_str(
        (enum sff_8636_compliance_revision)lo->status.revision_compliance));

    if (!lo->status.flags.flat_mem) {
        auto opt_pages = ident->mutable_optional_upper_pages();

        if (u00->options.device.page_01) {
            *opt_pages += (char)1;
        }

        if (u00->options.device.page_02) {
            *opt_pages += (char)2;
        }

        *opt_pages += (char)3; // !flat_mem implies page 3 support.

        if (u00->options.device.pages_20_21) {
            *opt_pages += (char)0x20;
            *opt_pages += (char)0x21;
        }
    }

    unsigned int power_class;
    if (u00->extended_identifier.power_class_8) {
        power_class = 8;
    } else {
        if (u00->extended_identifier.power_class_hi == 0) {
            power_class = u00->extended_identifier.power_class_lo + 1;
        } else {
            power_class = u00->extended_identifier.power_class_hi + 4;
        }
    }
    ident->set_power_class(power_class);

    ident->set_rx_cdr(u00->extended_identifier.rx_cdr != 0);
    ident->set_tx_cdr(u00->extended_identifier.tx_cdr != 0);

    ident->set_connector_type(sff_8024_connector_type_to_str(
        (enum sff_8024_connector_type)u00->connector_type));

    ident->set_encoding(sff_8024_encoding_to_str((enum sff_8024_encoding)u00->encoding));

    unsigned int baud_rate = u00->baud_rate;
    if (baud_rate == 0xff) {
        baud_rate = u00->extended_baud_rate * 250;
    } else {
        baud_rate *= 100;
    }
    ident->set_baud_rate(baud_rate);

    const struct sff_8636_upper_page_00_compliance* comp = &u00->compliance;
    auto spec_comp = ident->mutable_spec_compliance();

    if (comp->ethernet._40g_active_cable) {
        spec_comp->add_ethernet("40G Active Cable (XLPPI)");
    }
    if (comp->ethernet._40gbase_lr4) {
        spec_comp->add_ethernet("40GBASE-LR4");
    }
    if (comp->ethernet._40gbase_sr4) {
        spec_comp->add_ethernet("40GBASE-SR4");
    }
    if (comp->ethernet._40gbase_cr4) {
        spec_comp->add_ethernet("40GBASE-CR4");
    }
    if (comp->ethernet._10gbase_sr) {
        spec_comp->add_ethernet("10GBASE-SR");
    }
    if (comp->ethernet._10gbase_lr) {
        spec_comp->add_ethernet("10GBASE-LR");
    }
    if (comp->ethernet._10gbase_lrm) {
        spec_comp->add_ethernet("10GBASE-LRM");
    }
    if (comp->ethernet.extended) {
        spec_comp->add_ethernet(sff_8024_extended_compliance_to_str(
            (enum sff_8024_extended_compliance)u00->link_codes));
    }

    if (comp->sonet.oc48_sr) {
        spec_comp->add_sonet("OC 48 short reach");
    }
    if (comp->sonet.oc48_ir) {
        spec_comp->add_sonet("OC 48, intermediate reach");
    }
    if (comp->sonet.oc48_lr) {
        spec_comp->add_sonet("OC 48, long reach");
    }

    if (comp->sas._3_gbps) {
        spec_comp->add_sas("SAS 3.0 Gbps");
    }
    if (comp->sas._6_gbps) {
        spec_comp->add_sas("SAS 6.0 Gbps");
    }
    if (comp->sas._12_gbps) {
        spec_comp->add_sas("SAS 12.0 Gbps");
    }
    if (comp->sas._24_gbps) {
        spec_comp->add_sas("SAS 24.0 Gbps");
    }

    if (comp->gigabit_ethernet._1000base_sx) {
        spec_comp->add_gigabit_ethernet("1000BASE-SX");
    }
    if (comp->gigabit_ethernet._1000base_lx) {
        spec_comp->add_gigabit_ethernet("1000BASE-LX");
    }
    if (comp->gigabit_ethernet._1000base_cx) {
        spec_comp->add_gigabit_ethernet("1000BASE-CX");
    }
    if (comp->gigabit_ethernet._1000base_t) {
        spec_comp->add_gigabit_ethernet("1000BASE-T");
    }

    auto fibre = spec_comp->mutable_fibre();
    if (comp->fibre.link.length_medium) {
        fibre->add_length("Medium (M)");
    }
    if (comp->fibre.link.length_long) {
        fibre->add_length("Long distance (L)");
    }
    if (comp->fibre.link.length_intermediate) {
        fibre->add_length("Intermediate distance (I)");
    }
    if (comp->fibre.link.length_short) {
        fibre->add_length("Short distance (S)");
    }
    if (comp->fibre.link.length_very_long) {
        fibre->add_length("Very long distance (V)");
    }

    if (comp->fibre.link.tx_el_inter) {
        fibre->add_tx_technology("Electrical inter-enclosure (EL)");
    }
    if (comp->fibre.link.tx_el_intra) {
        fibre->add_tx_technology("Electrical intra-enclosure");
    }
    if (comp->fibre.link.tx_lc) {
        fibre->add_tx_technology("Longwave laser (LC)");
    }
    if (comp->fibre.link.tx_ll) {
        fibre->add_tx_technology("Longwave Laser (LL)");
    }
    if (comp->fibre.link.tx_sl) {
        fibre->add_tx_technology("Shortwave laser w OFC (SL)");
    }
    if (comp->fibre.link.tx_sn) {
        fibre->add_tx_technology("Shortwave laser w/o OFC (SN)");
    }

    if (comp->fibre.media.sm) {
        fibre->add_media("Single Mode (SM)");
    }
    if (comp->fibre.media.om3) {
        fibre->add_media("Multi-mode 50 um (OM3)");
    }
    if (comp->fibre.media.m5) {
        fibre->add_media("Multi-mode 50 um (M5)");
    }
    if (comp->fibre.media.m6) {
        fibre->add_media("Multi-mode 62.5 um (M6)");
    }
    if (comp->fibre.media.tv) {
        fibre->add_media("Video Coax (TV)");
    }
    if (comp->fibre.media.mi) {
        fibre->add_media("Miniature Coax (MI)");
    }
    if (comp->fibre.media.tp) {
        fibre->add_media("Shielded Twisted Pair (TP)");
    }
    if (comp->fibre.media.tw) {
        fibre->add_media("Twin Axial Pair (TW)");
    }

    if (comp->fibre.speed._100_MBps) {
        fibre->add_speed("100 MBps");
    }
    if (comp->fibre.speed._200_MBps) {
        fibre->add_speed("200 MBps");
    }
    if (comp->fibre.speed._3200_MBps) {
        fibre->add_speed("3200 MBps (per channel)");
    }
    if (comp->fibre.speed._400_MBps) {
        fibre->add_speed("400 MBps");
    }
    if (comp->fibre.speed._1600_MBps) {
        fibre->add_speed("1600 MBps (per channel)");
    }
    if (comp->fibre.speed._800_MBps) {
        fibre->add_speed("800 MBps");
    }
    if (comp->fibre.speed._1200_MBps) {
        fibre->add_speed("1200 MBps (per channel)");
    }
    if (comp->fibre.speed.extended) {
        fibre->add_speed(sff_8024_extended_compliance_to_str(
            (enum sff_8024_extended_compliance)u00->link_codes));
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_module_info(
    const ModuleInfoRequest& req,
    function<void(const ModuleInfoResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleInfoResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleInfoResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleInfoResponse resp;
            auto err = ErrorCode::EC_OK;

            ModulePages pages(&dev->cms, mod_id);
            if (pages.error()) {
                err = ErrorCode::EC_MODULE_PAGE_READ_FAILED;
            } else {
                auto info = resp.mutable_info();
                add_module_info_vendor(*info, pages);
                add_module_info_device(*info, pages);
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_module_info(
    const ModuleInfoRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_module_info(req, [&rdwr](const ModuleInfoResponse& resp) -> void {
        BatchResponse bresp;
        auto info = bresp.mutable_module_info();
        info->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetModuleInfo(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleInfoRequest* req,
    ServerWriter<ModuleInfoResponse>* writer) {
    get_module_info(*req, [&writer](const ModuleInfoResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_module_mem(
    const ModuleMemRequest& req,
    function<void(const ModuleMemResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleMemResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    auto rmem = req.mem();
    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        auto offset = rmem.offset();
        if (offset > UINT8_MAX) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_OFFSET);
            write_resp(resp);
            return;
        }

        auto page = rmem.page();
        if (page > UINT8_MAX) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_PAGE);
            write_resp(resp);
            return;
        }

        auto count = rmem.count();
        if (count < 1) {
            count = CMS_MODULE_PAGE_SIZE;
        } else if (count > UINT8_MAX - offset + 1) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_COUNT);
            write_resp(resp);
            return;
        }

        struct cms_module_id cms_id = {
            .cage = 0,
            .page = (uint8_t)page,
            .upper = false,
            .cmis = false,
            .bank = 0,
            .sfp_diag = false,
        };

        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleMemResponse resp;
            auto err = ErrorCode::EC_OK;

            /*
             * The CMS interface doesn't expose the I2C sequential read operation for arbitrary
             * sizes. To ensure that multi-byte registers are read consistently (in particular,
             * 16-bit monitor registers require a 2 byte read to ensure coherency with the
             * underlying sensor A/D), use the CMS page-sized block read and then extract the
             * relevant subset of bytes requested.
             */
            cms_id.cage = (uint8_t)mod_id;
            if (count > 1) {
                uint8_t page_offset = offset & (CMS_MODULE_PAGE_SIZE - 1);
                uint8_t data_count = MIN(count, CMS_MODULE_PAGE_SIZE - page_offset);
                uint8_t data_offset = 0;
                uint8_t data[count];

                cms_id.upper = offset >= CMS_MODULE_PAGE_SIZE;
                do {
                    auto cms_page = cms_module_page_read(&dev->cms, &cms_id);
                    if (cms_page == NULL) {
                        err = ErrorCode::EC_MODULE_PAGE_READ_FAILED;
                        goto write_response;
                    }
                    memcpy(&data[data_offset], &cms_page->u8[page_offset], data_count);
                    cms_module_page_free(cms_page);

                    cms_id.upper = true;
                    page_offset = 0;
                    data_offset += data_count;
                    data_count = count - data_offset;
                } while (data_count > 0);

                auto mem = resp.mutable_mem();
                mem->set_offset(offset);
                mem->set_page(page);
                mem->set_count(count);
                mem->set_data(data, count);
            } else {
                // Perform a single byte read.
                uint8_t data;
                if (!cms_module_byte_read(&dev->cms, &cms_id, offset, &data)) {
                    err = ErrorCode::EC_MODULE_MEM_READ_FAILED;
                    goto write_response;
                }

                auto mem = resp.mutable_mem();
                mem->set_offset(offset);
                mem->set_page(page);
                mem->set_count(1);
                mem->set_data(&data, 1);
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_module_mem(
    const ModuleMemRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_module_mem(req, [&rdwr](const ModuleMemResponse& resp) -> void {
        BatchResponse bresp;
        auto mem = bresp.mutable_module_mem();
        mem->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetModuleMem(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleMemRequest* req,
    ServerWriter<ModuleMemResponse>* writer) {
    get_module_mem(*req, [&writer](const ModuleMemResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_module_mem(
    const ModuleMemRequest& req,
    function<void(const ModuleMemResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleMemResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    auto rmem = req.mem();
    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        auto offset = rmem.offset();
        if (offset > UINT8_MAX) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_OFFSET);
            write_resp(resp);
            return;
        }

        auto page = rmem.page();
        if (page > UINT8_MAX) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_PAGE);
            write_resp(resp);
            return;
        }

        auto data = rmem.data();
        auto count = data.size();
        if (count > UINT8_MAX - offset + 1) {
            ModuleMemResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_MEM_COUNT);
            write_resp(resp);
            return;
        }

        struct cms_module_id cms_id = {
            .cage = 0,
            .page = (uint8_t)page,
            .upper = false,
            .cmis = false,
            .bank = 0,
            .sfp_diag = false,
        };

        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleMemResponse resp;
            auto err = ErrorCode::EC_OK;

            cms_id.cage = (uint8_t)mod_id;
            for (unsigned int b = 0; b < count; ++b) {
                if (!cms_module_byte_write(&dev->cms, &cms_id, offset + b, data[b])) {
                    err = ErrorCode::EC_MODULE_MEM_WRITE_FAILED;
                    goto write_response;
                }
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_module_mem(
    const ModuleMemRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_module_mem(req, [&rdwr](const ModuleMemResponse& resp) -> void {
        BatchResponse bresp;
        auto mem = bresp.mutable_module_mem();
        mem->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetModuleMem(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleMemRequest* req,
    ServerWriter<ModuleMemResponse>* writer) {
    set_module_mem(*req, [&writer](const ModuleMemResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
struct GetModuleStatsContext {
    ModuleStatus* status;
};

extern "C" {
    static int __get_module_stats(const struct stats_for_each_spec* spec) {
        GetModuleStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);
        if (cms_module_stats_is_alarm_metric(spec->metric)) {
            auto alarm = ctx->status->add_alarms();
            alarm->set_name(spec->metric->name);
            alarm->set_active(spec->values->u64 != 0);
        } else if (cms_module_stats_is_monitor_metric(spec->metric)) {
            auto mon = ctx->status->add_monitors();
            mon->set_name(spec->metric->name);
            mon->set_value(spec->values->f64);
        }

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_module_status(
    const ModuleStatusRequest& req,
    function<void(const ModuleStatusResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        ModuleStatusResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_mod_id = 0;
        int end_mod_id = dev->nports - 1;
        int mod_id = req.mod_id(); // 0-based index. -1 means all modules.
        if (mod_id > end_mod_id) {
            ModuleStatusResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_MODULE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (mod_id > -1) {
            begin_mod_id = mod_id;
            end_mod_id = mod_id;
        }

        auto& zones = dev->stats.zones[DeviceStatsZone::MODULE_MONITORS];
        for (mod_id = begin_mod_id; mod_id <= end_mod_id; ++mod_id) {
            ModuleStatusResponse resp;
            GetModuleStatsContext ctx = {
                .status = resp.mutable_status(),
            };

            stats_zone_for_each_metric(zones[mod_id]->zone, __get_module_stats, &ctx);

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_mod_id(mod_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_module_status(
    const ModuleStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_module_status(req, [&rdwr](const ModuleStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_module_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetModuleStatus(
    [[maybe_unused]] ServerContext* ctx,
    const ModuleStatusRequest* req,
    ServerWriter<ModuleStatusResponse>* writer) {
    get_module_status(*req, [&writer](const ModuleStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
