#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "switch.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_switch(Device* dev) {
    auto stats = new DeviceStats;
    stats->name = "switch";
    stats->zone = switch_stats_zone_alloc(dev->stats.domain, dev->bar2, stats->name.c_str());
    if (stats->zone == NULL) {
        cerr << "ERROR: Failed to alloc stats zone for switch." << endl;
        exit(EXIT_FAILURE);
    }

    dev->stats.sw = stats;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_switch(Device* dev) {
    auto stats = dev->stats.sw;
    dev->stats.sw = NULL;

    switch_stats_zone_free(stats->zone);
    delete stats;
}

//--------------------------------------------------------------------------------------------------
static bool interface_to_switch_id(const Device& dev,
                                   const SwitchInterfaceId& id,
                                   struct switch_interface_id& intf) {
    intf.index = id.index();
    switch (id.itype()) {
    case SwitchInterfaceType::SW_INTF_PORT:
        if (intf.index >= dev.nports) {
            return false;
        }
        intf.type = switch_interface_type_PORT;
        break;

    case SwitchInterfaceType::SW_INTF_HOST:
        if (intf.index >= dev.nhosts) {
            return false;
        }
        intf.type = switch_interface_type_HOST;
        break;

    case SwitchInterfaceType::SW_INTF_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool interface_from_switch_id(SwitchInterfaceId& id,
                                     const struct switch_interface_id& intf) {
    id.set_index(intf.index);
    switch (intf.type) {
    case switch_interface_type_PORT:
        id.set_itype(SwitchInterfaceType::SW_INTF_PORT);
        break;

    case switch_interface_type_HOST:
        id.set_itype(SwitchInterfaceType::SW_INTF_HOST);
        break;

    case switch_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool processor_to_switch_id(const Device& dev,
                                   const SwitchProcessorId& id,
                                   struct switch_processor_id& proc) {
    proc.index = id.index();
    switch (id.ptype()) {
    case SwitchProcessorType::SW_PROC_BYPASS:
        if (proc.index != 0) {
            return false;
        }
        proc.type = switch_processor_type_BYPASS;
        break;

    case SwitchProcessorType::SW_PROC_DROP:
        if (proc.index != 0) {
            return false;
        }
        proc.type = switch_processor_type_DROP;
        break;

    case SwitchProcessorType::SW_PROC_APP:
        if (proc.index >= dev.napps) {
            return false;
        }
        proc.type = switch_processor_type_APP;
        break;

    case SwitchProcessorType::SW_PROC_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool processor_from_switch_id(SwitchProcessorId& id,
                                     const struct switch_processor_id& proc) {
    id.set_index(proc.index);
    switch (proc.type) {
    case switch_processor_type_BYPASS:
        id.set_ptype(SwitchProcessorType::SW_PROC_BYPASS);
        break;

    case switch_processor_type_DROP:
        id.set_ptype(SwitchProcessorType::SW_PROC_DROP);
        break;

    case switch_processor_type_APP:
        id.set_ptype(SwitchProcessorType::SW_PROC_APP);
        break;

    case switch_processor_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode get_switch_ingress_config(const Device& dev,
                                           SwitchConfig& config,
                                           const struct switch_interface_id& intf) {
    volatile auto blk = &dev.bar2->smartnic_regs;

    struct switch_interface_id to_intf;
    if (!switch_get_ingress_source(blk, &intf, &to_intf)) {
        return ErrorCode::EC_FAILED_GET_IGR_SRC;
    }

    auto src = config.add_ingress_sources();
    if (!interface_from_switch_id(*src->mutable_from_intf(), intf)) {
        return ErrorCode::EC_UNSUPPORTED_IGR_SRC_FROM_INTF;
    }

    if (!interface_from_switch_id(*src->mutable_to_intf(), to_intf)) {
        return ErrorCode::EC_UNSUPPORTED_IGR_SRC_TO_INTF;
    }

    struct switch_processor_id to_proc;
    if (!switch_get_ingress_connection(blk, &intf, &to_proc)) {
        return ErrorCode::EC_FAILED_GET_IGR_CONN;
    }

    auto conn = config.add_ingress_connections();
    if (!interface_from_switch_id(*conn->mutable_from_intf(), intf)) {
        return ErrorCode::EC_UNSUPPORTED_IGR_CONN_FROM_INTF;
    }

    if (!processor_from_switch_id(*conn->mutable_to_proc(), to_proc)) {
        return ErrorCode::EC_UNSUPPORTED_IGR_CONN_TO_PROC;
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode get_switch_egress_config(const Device& dev,
                                          SwitchConfig& config,
                                          const struct switch_interface_id& intf,
                                          const enum switch_processor_type type) {
    volatile auto blk = &dev.bar2->smartnic_regs;
    struct switch_processor_id proc = {.type = type, .index = 0};
    unsigned int max_index = 0;

    switch (type) {
    case switch_processor_type_BYPASS:
        max_index = 1;
        break;

    case switch_processor_type_APP:
        max_index = dev.napps;
        break;

    case switch_processor_type_DROP:
    case switch_processor_type_UNKNOWN:
    default:
        break;
    }

    for (; proc.index < max_index; ++proc.index) {
        struct switch_interface_id to_intf;
        if (!switch_get_egress_connection(blk, &proc, &intf, &to_intf)) {
            return ErrorCode::EC_FAILED_GET_EGR_CONN;
        }

        auto conn = config.add_egress_connections();
        if (!processor_from_switch_id(*conn->mutable_on_proc(), proc)) {
            return ErrorCode::EC_UNSUPPORTED_EGR_CONN_ON_PROC;
        }

        if (!interface_from_switch_id(*conn->mutable_from_intf(), intf)) {
            return ErrorCode::EC_UNSUPPORTED_EGR_CONN_FROM_INTF;
        }

        if (!interface_from_switch_id(*conn->mutable_to_intf(), to_intf)) {
            return ErrorCode::EC_UNSUPPORTED_EGR_CONN_TO_INTF;
        }
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode get_switch_interface_config(const Device& dev,
                                             SwitchConfig& config,
                                             const enum switch_interface_type type) {
    struct switch_interface_id intf = {.type = type, .index = 0};
    unsigned int max_index = 0;

    switch (type) {
    case switch_interface_type_PORT:
        max_index = dev.nports;
        break;

    case switch_interface_type_HOST:
        max_index = dev.nhosts;
        break;

    case switch_interface_type_UNKNOWN:
    default:
        break;
    }

    for (; intf.index < max_index; ++intf.index) {
        auto err = get_switch_ingress_config(dev, config, intf);
        if (err != ErrorCode::EC_OK) {
            return err;
        }

        err = get_switch_egress_config(dev, config, intf, switch_processor_type_BYPASS);
        if (err != ErrorCode::EC_OK) {
            return err;
        }

        err = get_switch_egress_config(dev, config, intf, switch_processor_type_APP);
        if (err != ErrorCode::EC_OK) {
            return err;
        }
    }

    return ErrorCode::EC_OK;
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
        SwitchConfigResponse resp;
        auto config = resp.mutable_config();

        auto err = get_switch_interface_config(*dev, *config, switch_interface_type_PORT);
        if (err == ErrorCode::EC_OK) {
            err = get_switch_interface_config(*dev, *config, switch_interface_type_HOST);
        }

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

        for (auto src : config.ingress_sources()) {
            if (!src.has_from_intf()) {
                err = ErrorCode::EC_MISSING_IGR_SRC_FROM_INTF;
                goto write_response;
            }

            if (!src.has_to_intf()) {
                err = ErrorCode::EC_MISSING_IGR_SRC_TO_INTF;
                goto write_response;
            }

            struct switch_interface_id from_intf;
            if (!interface_to_switch_id(*dev, src.from_intf(), from_intf)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_SRC_FROM_INTF;
                goto write_response;
            }

            struct switch_interface_id to_intf;
            if (!interface_to_switch_id(*dev, src.to_intf(), to_intf)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_SRC_TO_INTF;
                goto write_response;
            }

            if (!switch_set_ingress_source(blk, &from_intf, &to_intf)) {
                err = ErrorCode::EC_FAILED_SET_IGR_SRC;
                goto write_response;
            }
        }

        for (auto conn : config.ingress_connections()) {
            if (!conn.has_from_intf()) {
                err = ErrorCode::EC_MISSING_IGR_CONN_FROM_INTF;
                goto write_response;
            }

            if (!conn.has_to_proc()) {
                err = ErrorCode::EC_MISSING_IGR_CONN_TO_PROC;
                goto write_response;
            }

            struct switch_interface_id from_intf;
            if (!interface_to_switch_id(*dev, conn.from_intf(), from_intf)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_CONN_FROM_INTF;
                goto write_response;
            }

            struct switch_processor_id to_proc;
            if (!processor_to_switch_id(*dev, conn.to_proc(), to_proc)) {
                err = ErrorCode::EC_UNSUPPORTED_IGR_CONN_TO_PROC;
                goto write_response;
            }

            if (!switch_set_ingress_connection(blk, &from_intf, &to_proc)) {
                err = ErrorCode::EC_FAILED_SET_IGR_CONN;
                goto write_response;
            }
        }

        for (auto conn : config.egress_connections()) {
            if (!conn.has_on_proc()) {
                err = ErrorCode::EC_MISSING_EGR_CONN_ON_PROC;
                goto write_response;
            }

            if (!conn.has_from_intf()) {
                err = ErrorCode::EC_MISSING_EGR_CONN_FROM_INTF;
                goto write_response;
            }

            if (!conn.has_to_intf()) {
                err = ErrorCode::EC_MISSING_EGR_CONN_TO_INTF;
                goto write_response;
            }

            struct switch_processor_id on_proc;
            if (!processor_to_switch_id(*dev, conn.on_proc(), on_proc)) {
                err = ErrorCode::EC_UNSUPPORTED_EGR_CONN_ON_PROC;
                goto write_response;
            }

            struct switch_interface_id from_intf;
            if (!interface_to_switch_id(*dev, conn.from_intf(), from_intf)) {
                err = ErrorCode::EC_UNSUPPORTED_EGR_CONN_FROM_INTF;
                goto write_response;
            }

            struct switch_interface_id to_intf;
            if (!interface_to_switch_id(*dev, conn.to_intf(), to_intf)) {
                err = ErrorCode::EC_UNSUPPORTED_EGR_CONN_TO_INTF;
                goto write_response;
            }

            if (!switch_set_egress_connection(blk, &on_proc, &from_intf, &to_intf)) {
                err = ErrorCode::EC_FAILED_SET_EGR_CONN;
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
struct GetSwitchStatsContext {
    Stats* stats;
};

extern "C" {
    static int __get_switch_stats_counters(const struct stats_for_each_spec* spec) {
        if (spec->metric->type != stats_metric_type_COUNTER) {
            return 0; // Skip non-counter metrics.
        }

        GetSwitchStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);
        auto cnt = ctx->stats->add_counters();
        cnt->set_domain(spec->domain->name);
        cnt->set_zone(spec->zone->name);
        cnt->set_block(spec->block->name);
        cnt->set_name(spec->metric->name);
        cnt->set_value(spec->value.u64);

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_switch_stats(
    const SwitchStatsRequest& req,
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

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];
        SwitchStatsResponse resp;
        GetSwitchStatsContext ctx = {
            .stats = resp.mutable_stats(),
        };

        stats_zone_for_each_metric(dev->stats.sw->zone, __get_switch_stats_counters, &ctx);

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_switch_stats(
    const SwitchStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_switch_stats(req, [&rdwr](const SwitchStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_switch_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetSwitchStats(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchStatsRequest* req,
    ServerWriter<SwitchStatsResponse>* writer) {
    get_switch_stats(*req, [&writer](const SwitchStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::clear_switch_stats(
    const SwitchStatsRequest& req,
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

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];
        stats_zone_clear_metrics(dev->stats.sw->zone);

        SwitchStatsResponse resp;
        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_switch_stats(
    const SwitchStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    clear_switch_stats(req, [&rdwr](const SwitchStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_switch_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::ClearSwitchStats(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchStatsRequest* req,
    ServerWriter<SwitchStatsResponse>* writer) {
    clear_switch_stats(*req, [&writer](const SwitchStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
