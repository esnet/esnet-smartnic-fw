#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_cfg_v2.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "flow_control.h"
#include "qdma.h"

using namespace grpc;
using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_host(Device* dev) {
    for (unsigned int host_id = 0; host_id < dev->nhosts; ++host_id) {
        // NOTE: Although the register block is named "cmac", it's actually used for host qdma.
        volatile typeof(dev->bar2->cmac_adapter0)* adapter;
        switch (host_id) {
        case 0: adapter = &dev->bar2->cmac_adapter0; break;
        case 1: adapter = &dev->bar2->cmac_adapter1; break;
        default:
            SERVER_LOG_LINE_INIT(host, ERROR,
                "Unsupported host ID " << host_id << " on device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        ostringstream name;
        name << "host" << host_id;

        auto stats = new DeviceStats;
        stats->name = name.str();
        stats->zone = qdma_stats_zone_alloc(
            dev->stats.domains[DeviceStatsDomain::COUNTERS], adapter, stats->name.c_str());
        if (stats->zone == NULL) {
            SERVER_LOG_LINE_INIT(host, ERROR,
                "Failed to alloc stats zone for host ID " << host_id <<
                " on device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        if (!control.stats_flags.test(ServerControlStatsFlag::CTRL_STATS_FLAG_ZONE_HOST_COUNTERS)) {
            stats_zone_disable(stats->zone);
        }
        dev->stats.zones[DeviceStatsZone::HOST_COUNTERS].push_back(stats);
        SERVER_LOG_LINE_INIT(host, INFO,
            "Setup counters for host ID " << host_id << " on device " << dev->bus_id);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_host(Device* dev) {
    auto zones = &dev->stats.zones[DeviceStatsZone::HOST_COUNTERS];
    while (!zones->empty()) {
        auto stats = zones->back();
        qdma_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }
}

//--------------------------------------------------------------------------------------------------
enum qdma_function& operator++(enum qdma_function& func) { // Prefix ++ for convenience.
    func = static_cast<enum qdma_function>(func + 1);
    return func;
}

//--------------------------------------------------------------------------------------------------
static bool convert_to_driver_qdma_function(const HostFunctionId& id, enum qdma_function& func) {
    switch (id.ftype()) {
    case HostFunctionType::HOST_FUNC_PHYSICAL:
        if (id.index() != 0) {
            return false;
        }
        func = qdma_function_PF;
        break;

    case HostFunctionType::HOST_FUNC_VIRTUAL:
        func = static_cast<typeof(func)>(qdma_function_VF_MIN + id.index());
        if (func >= qdma_function_VF_MAX) {
            return false;
        }
        break;

    case HostFunctionType::HOST_FUNC_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool convert_from_driver_qdma_function(HostFunctionId& id, enum qdma_function& func) {
    switch (func) {
    case qdma_function_PF:
        id.set_ftype(HostFunctionType::HOST_FUNC_PHYSICAL);
        id.set_index(0);
        break;

    case qdma_function_VF0:
    case qdma_function_VF1:
    case qdma_function_VF2:
        id.set_ftype(HostFunctionType::HOST_FUNC_VIRTUAL);
        id.set_index(func - qdma_function_VF_MIN);
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_host_config(
    const HostConfigRequest& req,
    function<void(const HostConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        HostConfigResponse resp;
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

        int begin_host_id = 0;
        int end_host_id = dev->nhosts - 1;
        int host_id = req.host_id(); // 0-based index. -1 means all host interfaces.
        if (host_id > end_host_id) {
            HostConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_HOST_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (host_id > -1) {
            begin_host_id = host_id;
            end_host_id = host_id;
        }

        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            auto config = resp.mutable_config();
            auto dma = config->mutable_dma();

            for (auto qf = qdma_function_MIN; qf < qdma_function_MAX; ++qf) {
                unsigned int base_queue;
                unsigned int num_queues;
                if (!qdma_function_get_queues(dev->bar2, host_id, qf,
                                              &base_queue, &num_queues)) {
                    err = ErrorCode::EC_FAILED_GET_HOST_FUNCTION_DMA_QUEUES;
                    goto write_response;
                }

                {
                    auto func = dma->add_functions();
                    auto func_id = func->mutable_func_id();
                    if (!convert_from_driver_qdma_function(*func_id, qf)) {
                        err = ErrorCode::EC_UNSUPPORTED_HOST_FUNCTION;
                        goto write_response;
                    }

                    func->set_base_queue(base_queue);
                    func->set_num_queues(num_queues);
                }

                {
                    struct fc_interface intf = {
                        .type = fc_interface_type_HOST,
                        .index = (unsigned int)host_id,
                    };
                    uint32_t threshold;
                    if (!fc_get_egress_threshold(&dev->bar2->smartnic_regs, &intf, &threshold)) {
                        err = ErrorCode::EC_FAILED_GET_HOST_FC_EGR_THRESHOLD;
                        goto write_response;
                    }

                    {
                        auto fc = config->mutable_flow_control();
                        fc->set_egress_threshold(
                            threshold == FC_THRESHOLD_UNLIMITED ? -1 : threshold);
                    }
                }
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_host_id(host_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_host_config(
    const HostConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_host_config(req, [&rdwr](const HostConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_host_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetHostConfig(
    [[maybe_unused]] ServerContext* ctx,
    const HostConfigRequest* req,
    ServerWriter<HostConfigResponse>* writer) {
    get_host_config(*req, [&writer](const HostConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_host_config(
    const HostConfigRequest& req,
    function<void(const HostConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        HostConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        HostConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_HOST_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_host_id = 0;
        int end_host_id = dev->nhosts - 1;
        int host_id = req.host_id(); // 0-based index. -1 means all host interfaces.
        if (host_id > end_host_id) {
            HostConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_HOST_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (host_id > -1) {
            begin_host_id = host_id;
            end_host_id = host_id;
        }

        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            if (config.has_dma()) {
                auto dma = config.dma();

                if (dma.reset()) {
                    // Reset the queue configuration for all functions.
                    for (auto qf = qdma_function_MIN; qf < qdma_function_MAX; ++qf) {
                        qdma_function_set_queues(dev->bar2, host_id, qf, 0, 0);
                    }
#ifdef SN_CFG_HOST_SET_QDMA_CHANNEL
                    qdma_channel_set_queues(dev->bar2, host_id, 0, 0);
#endif
                }

                for (auto func : dma.functions()) {
                    enum qdma_function qf;
                    if (!convert_to_driver_qdma_function(func.func_id(), qf)) {
                        err = ErrorCode::EC_UNSUPPORTED_HOST_FUNCTION;
                        goto write_response;
                    }

                    if (!qdma_function_set_queues(dev->bar2, host_id, qf,
                                                  func.base_queue(), func.num_queues())) {
                        err = ErrorCode::EC_FAILED_SET_HOST_FUNCTION_DMA_QUEUES;
                        goto write_response;
                    }
                }

#ifdef SN_CFG_HOST_SET_QDMA_CHANNEL
                unsigned int chan_base = UINT_MAX;
                unsigned int chan_queues = 0;
                for (auto qf = qdma_function_MIN; qf < qdma_function_MAX; ++qf) {
                    unsigned int func_base;
                    unsigned int func_queues;
                    if (!qdma_function_get_queues(dev->bar2, host_id, qf,
                                                  &func_base, &func_queues)) {
                        err = ErrorCode::EC_FAILED_SET_HOST_FUNCTION_DMA_QUEUES;
                        goto write_response;
                    }

                    if (func_queues > 0) {
                        chan_base = min(chan_base, func_base);
                        chan_queues += func_queues;
                    }
                }

                if (chan_queues > 0) {
                    qdma_channel_set_queues(dev->bar2, host_id, chan_base, chan_queues);
                }
#endif
            }

            if (config.has_flow_control()) {
                auto fc = config.flow_control();
                auto et = fc.egress_threshold();

                struct fc_interface intf = {
                    .type = fc_interface_type_HOST,
                    .index = (unsigned int)host_id,
                };
                uint32_t threshold = et < 0 ? FC_THRESHOLD_UNLIMITED : et;
                if (!fc_set_egress_threshold(&dev->bar2->smartnic_regs, &intf, threshold)) {
                    err = ErrorCode::EC_FAILED_SET_HOST_FC_EGR_THRESHOLD;
                    goto write_response;
                }
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_host_id(host_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_host_config(
    const HostConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_host_config(req, [&rdwr](const HostConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_host_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetHostConfig(
    [[maybe_unused]] ServerContext* ctx,
    const HostConfigRequest* req,
    ServerWriter<HostConfigResponse>* writer) {
    set_host_config(*req, [&writer](const HostConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_or_clear_host_stats(
    const HostStatsRequest& req,
    bool do_clear,
    function<void(const HostStatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        HostStatsResponse resp;
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

        int begin_host_id = 0;
        int end_host_id = dev->nhosts - 1;
        int host_id = req.host_id(); // 0-based index. -1 means all hosts.
        if (host_id > end_host_id) {
            HostStatsResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_HOST_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (host_id > -1) {
            begin_host_id = host_id;
            end_host_id = host_id;
        }

        auto& zones = dev->stats.zones[DeviceStatsZone::HOST_COUNTERS];
        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostStatsResponse resp;

            if (do_clear) {
                clear_stats_zone(zones[host_id]->zone, ctx.filters);
            } else {
                ctx.stats = resp.mutable_stats();
                stats_zone_for_each_metric(zones[host_id]->zone, get_stats_for_each_metric, &ctx);
            }

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_host_id(host_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_host_stats(
    const HostStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_host_stats(req, false, [&rdwr](const HostStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_host_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetHostStats(
    [[maybe_unused]] ServerContext* ctx,
    const HostStatsRequest* req,
    ServerWriter<HostStatsResponse>* writer) {
    get_or_clear_host_stats(*req, false, [&writer](const HostStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_host_stats(
    const HostStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_host_stats(req, true, [&rdwr](const HostStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_host_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::ClearHostStats(
    [[maybe_unused]] ServerContext* ctx,
    const HostStatsRequest* req,
    ServerWriter<HostStatsResponse>* writer) {
    get_or_clear_host_stats(*req, true, [&writer](const HostStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
