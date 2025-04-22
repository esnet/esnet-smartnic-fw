#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <cstdlib>

#include <grpc/grpc.h>
#include "sn_cfg_v2.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "switch.h"

using namespace grpc;
using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_switch(Device* dev) {
    auto stats = new DeviceStats;
    stats->name = "switch";
    stats->zone = switch_stats_zone_alloc(
        dev->stats.domains[DeviceStatsDomain::COUNTERS], dev->bar2, stats->name.c_str());
    if (stats->zone == NULL) {
        SERVER_LOG_LINE_INIT(switch, ERROR,
            "Failed to alloc stats zone for switch on device " << dev->bus_id);
        exit(EXIT_FAILURE);
    }

    dev->stats.zones[DeviceStatsZone::SWITCH_COUNTERS].push_back(stats);
    SERVER_LOG_LINE_INIT(switch, INFO, "Setup counters for switch on device " << dev->bus_id);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_switch(Device* dev) {
    auto zones = &dev->stats.zones[DeviceStatsZone::SWITCH_COUNTERS];
    while (!zones->empty()) {
        auto stats = zones->back();
        switch_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }
}

//--------------------------------------------------------------------------------------------------
template<typename SEL_TYPE>
static bool convert_to_driver_interface(const SEL_TYPE& sel, enum switch_interface& intf) {
    switch (sel.intf()) {
    case SwitchInterface::SW_INTF_PHYSICAL:
        intf = switch_interface_PHYSICAL;
        break;

    case SwitchInterface::SW_INTF_TEST:
        intf = switch_interface_TEST;
        break;

    case SwitchInterface::SW_INTF_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
template<typename SEL_TYPE>
static bool convert_from_driver_interface(SEL_TYPE& sel, enum switch_interface intf) {
    switch (intf) {
    case switch_interface_PHYSICAL:
        sel.set_intf(SwitchInterface::SW_INTF_PHYSICAL);
        break;

    case switch_interface_TEST:
        sel.set_intf(SwitchInterface::SW_INTF_TEST);
        break;

    case switch_interface_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool convert_to_driver_destination(const SwitchIngressSelector& sel,
                                          enum switch_destination& dest) {
    switch (sel.dest()) {
    case SwitchDestination::SW_DEST_BYPASS:
        dest = switch_destination_BYPASS;
        break;

    case SwitchDestination::SW_DEST_DROP:
        dest = switch_destination_DROP;
        break;

    case SwitchDestination::SW_DEST_APP:
        dest = switch_destination_APP;
        break;

    case SwitchDestination::SW_DEST_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool convert_from_driver_destination(SwitchIngressSelector& sel,
                                            enum switch_destination dest) {
    switch (dest) {
    case switch_destination_BYPASS:
        sel.set_dest(SwitchDestination::SW_DEST_BYPASS);
        break;

    case switch_destination_DROP:
        sel.set_dest(SwitchDestination::SW_DEST_DROP);
        break;

    case switch_destination_APP:
        sel.set_dest(SwitchDestination::SW_DEST_APP);
        break;

    case switch_destination_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool convert_to_driver_bypass_mode(const SwitchConfig& config,
                                          enum switch_bypass_mode& mode) {
    switch (config.bypass_mode()) {
    case SwitchBypassMode::SW_BYPASS_STRAIGHT:
        mode = switch_bypass_mode_STRAIGHT;
        break;

    case SwitchBypassMode::SW_BYPASS_SWAP:
        mode = switch_bypass_mode_SWAP;
        break;

    case SwitchBypassMode::SW_BYPASS_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool convert_from_driver_bypass_mode(SwitchConfig& config,
                                            const enum switch_bypass_mode mode) {
    switch (mode) {
    case switch_bypass_mode_STRAIGHT:
        config.set_bypass_mode(SwitchBypassMode::SW_BYPASS_STRAIGHT);
        break;

    case switch_bypass_mode_SWAP:
        config.set_bypass_mode(SwitchBypassMode::SW_BYPASS_SWAP);
        break;

    case switch_bypass_mode_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_switch_config(
    const SwitchConfigRequest& req,
    function<void(const SwitchConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        SwitchConfigResponse resp;
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
        volatile auto blk = &dev->bar2->smartnic_regs;
        auto err = ErrorCode::EC_OK;
        SwitchConfigResponse resp;
        auto config = resp.mutable_config();

        for (unsigned int port_id = 0; port_id < dev->nports; ++port_id) {
            struct switch_ingress_selector is;
            is.index = port_id;
            if (!switch_get_ingress_selector(blk, &is)) {
                err = ErrorCode::EC_FAILED_GET_IGR_SEL;
                goto write_response;
            }

            {
                auto sel = config->add_ingress_selectors();
                sel->set_index(is.index);

                if (!convert_from_driver_interface<typeof(*sel)>(*sel, is.intf)) {
                    err = ErrorCode::EC_UNSUPPORTED_IGR_SEL_INTF;
                    goto write_response;
                }

                if (!convert_from_driver_destination(*sel, is.dest)) {
                    err = ErrorCode::EC_UNSUPPORTED_IGR_SEL_DEST;
                    goto write_response;
                }
            }

            struct switch_egress_selector es;
            es.index = port_id;
            if (!switch_get_egress_selector(blk, &es)) {
                err = ErrorCode::EC_FAILED_GET_EGR_SEL;
                goto write_response;
            }

            {
                auto sel = config->add_egress_selectors();
                sel->set_index(es.index);

                if (!convert_from_driver_interface<typeof(*sel)>(*sel, es.intf)) {
                    err = ErrorCode::EC_UNSUPPORTED_EGR_SEL_INTF;
                    goto write_response;
                }
            }
        }

        {
            enum switch_bypass_mode mode;
            if (!switch_get_bypass_mode(blk, &mode)) {
                err = ErrorCode::EC_FAILED_GET_BYPASS_MODE;
                goto write_response;
            }

            if (!convert_from_driver_bypass_mode(*config, mode)) {
                err = ErrorCode::EC_UNSUPPORTED_BYPASS_MODE;
                goto write_response;
            }
        }

    write_response:
        resp.set_error_code(err);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_switch_config(
    const SwitchConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_switch_config(req, [&rdwr](const SwitchConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_switch_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetSwitchConfig(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchConfigRequest* req,
    ServerWriter<SwitchConfigResponse>* writer) {
    get_switch_config(*req, [&writer](const SwitchConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_switch_config(
    const SwitchConfigRequest& req,
    function<void(const SwitchConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        SwitchConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        SwitchConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_SWITCH_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];
        volatile auto blk = &dev->bar2->smartnic_regs;
        auto err = ErrorCode::EC_OK;
        SwitchConfigResponse resp;

        for (auto sel : config.ingress_selectors()) {
            struct switch_ingress_selector is;
            is.index = sel.index();

            if (!convert_to_driver_interface<typeof(sel)>(sel, is.intf)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_SEL_INTF;
                goto write_response;
            }

            if (!convert_to_driver_destination(sel, is.dest)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_SEL_DEST;
                goto write_response;
            }

            if (!switch_set_ingress_selector(blk, &is)) {
                err = ErrorCode::EC_FAILED_SET_IGR_SEL;
                goto write_response;
            }
        }

        for (auto sel : config.egress_selectors()) {
            struct switch_egress_selector es;
            es.index = sel.index();

            if (!convert_to_driver_interface<typeof(sel)>(sel, es.intf)) {
                err = ErrorCode::EC_UNSUPPORTED_EGR_SEL_INTF;
                goto write_response;
            }

            if (!switch_set_egress_selector(blk, &es)) {
                err = ErrorCode::EC_FAILED_SET_EGR_SEL;
                goto write_response;
            }
        }

        if (config.bypass_mode() != SwitchBypassMode::SW_BYPASS_UNKNOWN) {
            enum switch_bypass_mode mode;
            if (!convert_to_driver_bypass_mode(config, mode)) {
                err = ErrorCode::EC_UNSUPPORTED_BYPASS_MODE;
                goto write_response;
            }

            if (!switch_set_bypass_mode(blk, mode)) {
                err = ErrorCode::EC_FAILED_SET_BYPASS_MODE;
                goto write_response;
            }
        }

    write_response:
        resp.set_error_code(err);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_switch_config(
    const SwitchConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_switch_config(req, [&rdwr](const SwitchConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_switch_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetSwitchConfig(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchConfigRequest* req,
    ServerWriter<SwitchConfigResponse>* writer) {
    set_switch_config(*req, [&writer](const SwitchConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_or_clear_switch_stats(
    const SwitchStatsRequest& req,
    bool do_clear,
    function<void(const SwitchStatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        SwitchStatsResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    GetStatsContext ctx{
        .filters = req.filters(),
        .stats = NULL,
    };

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];
        auto& zones = dev->stats.zones[DeviceStatsZone::SWITCH_COUNTERS];
        SwitchStatsResponse resp;

        if (do_clear) {
            stats_zone_clear_metrics(zones[0]->zone);
        } else {
            ctx.stats = resp.mutable_stats();
            stats_zone_for_each_metric(zones[0]->zone, get_stats_for_each_metric, &ctx);
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_switch_stats(
    const SwitchStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_switch_stats(req, false, [&rdwr](const SwitchStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_switch_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetSwitchStats(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchStatsRequest* req,
    ServerWriter<SwitchStatsResponse>* writer) {
    get_or_clear_switch_stats(*req, false, [&writer](const SwitchStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_switch_stats(
    const SwitchStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_switch_stats(req, true, [&rdwr](const SwitchStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_switch_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::ClearSwitchStats(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchStatsRequest* req,
    ServerWriter<SwitchStatsResponse>* writer) {
    get_or_clear_switch_stats(*req, true, [&writer](const SwitchStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
