#include "agent.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <grpc/grpc.h>
#include "sn_cfg_v2.grpc.pb.h"

#include "cms.h"
#include "esnet_smartnic_toplevel.h"
#include "syscfg_block.h"
#include "sysmon.h"

using namespace grpc;
using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
const char* device_stats_domain_name(DeviceStatsDomain dom) {
    switch (dom) {
    case DeviceStatsDomain::COUNTERS: return "counters_stats";
    case DeviceStatsDomain::MONITORS: return "monitors_stats";
    case DeviceStatsDomain::MODULES: return "modules_stats";

    case DeviceStatsDomain::NDOMAINS:
        break;
    }
    return "UNKNOWN";
}

//--------------------------------------------------------------------------------------------------
static uint16_t read_hex_pci_id(const string& bus_id, const string& file) {
    string path = "/sys/bus/pci/devices/" + bus_id + '/' + file;
    ifstream in(path, ios::in);
    if (!in.is_open()) {
        SERVER_LOG_LINE_INIT(device, ERROR, "Failed to open file '" << path << "'");
        return 0xffff;
    }

    ostringstream out;
    out << in.rdbuf();
    if (in.fail()) {
        SERVER_LOG_LINE_INIT(device, ERROR, "Failed to read file '" << path << "'");
        return 0xffff;
    }

    return stoi(out.str(), 0, 16);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_device(Device* dev) {
    sysmon_master_reset(&dev->bar2->sysmon0);

    struct sysmon_info {
        volatile struct sysmon_block* blk;
        uint64_t select_mask;
    } sysmons[] = {
        {
            .blk = &dev->bar2->sysmon0, // master
            .select_mask =
                SYSMON_CHANNEL_MASK(TEMP) |
                SYSMON_CHANNEL_MASK(VCCINT) |
                SYSMON_CHANNEL_MASK(VCCAUX) |
                SYSMON_CHANNEL_MASK(VCCBRAM) |
                SYSMON_CHANNEL_MASK(VP_VN),
        },
        {
            .blk = &dev->bar2->sysmon1, // slave0
            .select_mask =
                SYSMON_CHANNEL_MASK(TEMP) |
                SYSMON_CHANNEL_MASK(VCCINT) |
                SYSMON_CHANNEL_MASK(VCCAUX) |
                SYSMON_CHANNEL_MASK(VCCBRAM) |
                SYSMON_CHANNEL_MASK(VUSER0),
        },
        {
            .blk = &dev->bar2->sysmon2, // slave1
            .select_mask =
                SYSMON_CHANNEL_MASK(TEMP) |
                SYSMON_CHANNEL_MASK(VCCINT) |
                SYSMON_CHANNEL_MASK(VCCAUX) |
                SYSMON_CHANNEL_MASK(VCCBRAM) |
                SYSMON_CHANNEL_MASK(VUSER0),
        },
    };

    unsigned int n = 0;
    for (auto info : sysmons) {
        SERVER_LOG_LINE_INIT(device, INFO,
            "Initializing sysmon " << n << " on device " << dev->bus_id);

        ostringstream name;
        name << "sysmon" << n;
        n += 1;

        sysmon_sequencer_enable(info.blk, info.select_mask, 0);

        auto stats = new DeviceStats;
        stats->name = name.str();
        stats->zone = sysmon_stats_zone_alloc(
            dev->stats.domains[DeviceStatsDomain::MONITORS], info.blk, stats->name.c_str());
        if (stats->zone == NULL) {
            SERVER_LOG_LINE_INIT(device, ERROR,
                "Failed to alloc sysmon " << n << " stats zone for device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        dev->stats.zones[DeviceStatsZone::SYSMON_MONITORS].push_back(stats);
        SERVER_LOG_LINE_INIT(device, INFO,
            "Setup monitors for sysmon " << n << " on device " << dev->bus_id);
    }

    SERVER_LOG_LINE_INIT(device, INFO, "Initializing CMS on device " << dev->bus_id);
    dev->cms.blk = &dev->bar2->cms;
    cms_init(&dev->cms);
    if (!cms_reset(&dev->cms)) {
        SERVER_LOG_LINE_INIT(device, ERROR, "Failed to reset CMS for device " << dev->bus_id);
        exit(EXIT_FAILURE);
    }

    auto stats = new DeviceStats;
    stats->name = "card";
    stats->zone = cms_card_stats_zone_alloc(
        dev->stats.domains[DeviceStatsDomain::MONITORS], &dev->cms, stats->name.c_str());
    if (stats->zone == NULL) {
        SERVER_LOG_LINE_INIT(device, ERROR,
            "Failed to alloc CMS stats zone for device " << dev->bus_id);
        exit(EXIT_FAILURE);
    }
    dev->stats.zones[DeviceStatsZone::CARD_MONITORS].push_back(stats);
    SERVER_LOG_LINE_INIT(device, INFO, "Setup monitors for CMS on device " << dev->bus_id);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_device(Device* dev) {
    auto zones = &dev->stats.zones[DeviceStatsZone::SYSMON_MONITORS];
    while (!zones->empty()) {
        auto stats = zones->back();
        sysmon_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }

    zones = &dev->stats.zones[DeviceStatsZone::CARD_MONITORS];
    while (!zones->empty()) {
        auto stats = zones->back();
        cms_card_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }

    cms_destroy(&dev->cms);
}

//--------------------------------------------------------------------------------------------------
static void card_info_add_mac_addr(DeviceCardInfo* card, const struct ether_addr* addr) {
    const uint8_t* octets = addr->ether_addr_octet;
    char str[17 + 1];
    snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
             octets[0], octets[1], octets[2], octets[3], octets[4], octets[5]);
    card->add_mac_addrs(str);
}

//--------------------------------------------------------------------------------------------------
static ErrorCode add_card_info(Device* dev, DeviceInfo* info) {
    auto ci = cms_card_info_read(&dev->cms);
    if (ci == NULL) {
        return ErrorCode::EC_CARD_INFO_READ_FAILED;
    }

    auto card = info->mutable_card();
    card->set_name(ci->name);
    card->set_profile(cms_profile_to_str(ci->profile));
    card->set_serial_number(ci->serial_number);
    card->set_revision(ci->revision);
    card->set_sc_version(ci->sc_version);

    card->set_fan_presence(&ci->fan_presence, 1);
    card->set_total_power_avail(ci->total_power_avail);
    card->set_config_mode(cms_card_info_config_mode_to_str(ci->config_mode));

    for (unsigned int n = 0; n < CMS_CARD_INFO_MAX_CAGES; ++n) {
        if (CMS_CARD_INFO_CAGE_IS_VALID(ci, n)) {
            card->add_cage_types(cms_card_info_cage_type_to_str(ci->cage.types[n]));
        }
    }

    if (ci->mac.block.count > 0) {
        struct ether_addr addr = ci->mac.block.base;
        for (unsigned int n = 0; n < ci->mac.block.count; ++n) {
            card_info_add_mac_addr(card, &addr);
            addr.ether_addr_octet[5] += 1; // TODO: handle range check and wrapping?
        }
    } else {
        for (unsigned int n = 0; n < CMS_CARD_INFO_MAX_LEGACY_MACS; ++n) {
            if (CMS_CARD_INFO_LEGACY_MAC_IS_VALID(ci, n)) {
                card_info_add_mac_addr(card, &ci->mac.legacy.addrs[n]);
            }
        }
    }

    cms_card_info_free(ci);

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_device_info(
    const DeviceInfoRequest& req,
    function<void(const DeviceInfoResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        DeviceInfoResponse resp;
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
        auto err = ErrorCode::EC_OK;
        DeviceInfoResponse resp;
        auto info = resp.mutable_info();

        auto pci = info->mutable_pci();
        pci->set_bus_id(dev->bus_id);
        pci->set_vendor_id(read_hex_pci_id(dev->bus_id, "vendor"));
        pci->set_device_id(read_hex_pci_id(dev->bus_id, "device"));

        auto build = info->mutable_build();
        volatile auto syscfg = &dev->bar2->syscfg;

        build->set_number(syscfg->usr_access);
        build->set_status(syscfg->build_status);
        for (auto d = 0; d < SYSCFG_DNA_COUNT; ++d) {
            build->add_dna(syscfg->dna[d]);
        }

        auto hw_ver = getenv("SN_HW_VER");
        auto fw_ver = getenv("SN_FW_VER");
        auto sw_ver = getenv("SN_SW_VER");
        auto env = build->mutable_env();
        env->set_hw_version(hw_ver != NULL ? hw_ver : "NONE");
        env->set_fw_version(fw_ver != NULL ? fw_ver : "NONE");
        env->set_sw_version(sw_ver != NULL ? sw_ver : "NONE");

        err = add_card_info(dev, info);

        resp.set_error_code(err);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_device_info(
    const DeviceInfoRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_device_info(req, [&rdwr](const DeviceInfoResponse& resp) -> void {
        BatchResponse bresp;
        auto info = bresp.mutable_device_info();
        info->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetDeviceInfo(
    [[maybe_unused]] ServerContext* ctx,
    const DeviceInfoRequest* req,
    ServerWriter<DeviceInfoResponse>* writer) {
    get_device_info(*req, [&writer](const DeviceInfoResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
struct GetDeviceStatsContext {
    DeviceStatus* status;
};

extern "C" {
    static int __get_device_stats(const struct stats_for_each_spec* spec) {
        GetDeviceStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);
        switch (spec->metric->type) {
        case stats_metric_type_FLAG: {
            auto alarm = ctx->status->add_alarms();
            alarm->set_source(spec->zone->name);
            alarm->set_name(spec->metric->name);
            alarm->set_active(spec->values->u64 != 0);
            break;
        }

        case stats_metric_type_GAUGE: {
            auto mon = ctx->status->add_monitors();
            mon->set_source(spec->zone->name);
            mon->set_name(spec->metric->name);
            mon->set_value(spec->values->f64);
            break;
        }

        default:
            break;
        }

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_device_status(
    const DeviceStatusRequest& req,
    function<void(const DeviceStatusResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        DeviceStatusResponse resp;
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
        DeviceStatusResponse resp;
        GetDeviceStatsContext ctx = {
            .status = resp.mutable_status(),
        };

        for (auto sysmon : dev->stats.zones[DeviceStatsZone::SYSMON_MONITORS]) {
            stats_zone_for_each_metric(sysmon->zone, __get_device_stats, &ctx);
        }
        stats_zone_for_each_metric(
            dev->stats.zones[DeviceStatsZone::CARD_MONITORS][0]->zone, __get_device_stats, &ctx);

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_device_status(
    const DeviceStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_device_status(req, [&rdwr](const DeviceStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_device_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetDeviceStatus(
    [[maybe_unused]] ServerContext* ctx,
    const DeviceStatusRequest* req,
    ServerWriter<DeviceStatusResponse>* writer) {
    get_device_status(*req, [&writer](const DeviceStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
