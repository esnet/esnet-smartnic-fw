#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <cstdlib>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

#include "cmac.h"
#include "esnet_smartnic_toplevel.h"

using namespace grpc;
using namespace sn_cfg::v1;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_port(Device* dev) {
    for (unsigned int port_id = 0; port_id < dev->nports; ++port_id) {
        volatile typeof(dev->bar2->cmac0)* cmac;
        switch (port_id) {
        case 0: cmac = &dev->bar2->cmac0; break;
        case 1: cmac = &dev->bar2->cmac1; break;
        default:
            cerr << "ERROR: Unsupported port ID " << port_id << "." << endl;
            exit(EXIT_FAILURE);
        }

        ostringstream name;
        name << "port" << port_id;

        auto stats = new DeviceStats;
        stats->name = name.str();
        stats->zone = cmac_stats_zone_alloc(
            dev->stats.domains[DeviceStatsDomain::COUNTERS], cmac, stats->name.c_str());
        if (stats->zone == NULL) {
            cerr << "ERROR: Failed to alloc stats zone for port ID " << port_id << "."  << endl;
            exit(EXIT_FAILURE);
        }

        dev->stats.zones[DeviceStatsZone::PORT_COUNTERS].push_back(stats);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_port(Device* dev) {
    auto zones = &dev->stats.zones[DeviceStatsZone::PORT_COUNTERS];
    while (!zones->empty()) {
        auto stats = zones->back();
        cmac_stats_zone_free(stats->zone);

        zones->pop_back();
        delete stats;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_port_config(
    const PortConfigRequest& req,
    function<void(const PortConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortConfigResponse resp;
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

        int begin_port_id = 0;
        int end_port_id = dev->nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            volatile typeof(dev->bar2->cmac0)* cmac;
            switch (port_id) {
            case 0: cmac = &dev->bar2->cmac0; break;
            case 1: cmac = &dev->bar2->cmac1; break;
            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_ID;
                goto write_response;
            }

            {
                auto config = resp.mutable_config();
                config->set_state(cmac_is_enabled(cmac) ?
                                  PortState::PORT_STATE_ENABLE :
                                  PortState::PORT_STATE_DISABLE);
                config->set_fec(cmac_rsfec_is_enabled(cmac) ?
                                PortFec::PORT_FEC_REED_SOLOMON :
                                PortFec::PORT_FEC_NONE);
                config->set_loopback(cmac_loopback_is_enabled(cmac) ?
                                     PortLoopback::PORT_LOOPBACK_NEAR_END_PMA :
                                     PortLoopback::PORT_LOOPBACK_NONE);
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_port_config(
    const PortConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_port_config(req, [&rdwr](const PortConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_port_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetPortConfig(
    [[maybe_unused]] ServerContext* ctx,
    const PortConfigRequest* req,
    ServerWriter<PortConfigResponse>* writer) {
    get_port_config(*req, [&writer](const PortConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_port_config(
    const PortConfigRequest& req,
    function<void(const PortConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        PortConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_PORT_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_port_id = 0;
        int end_port_id = dev->nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            volatile typeof(dev->bar2->cmac0)* cmac;
            switch (port_id) {
            case 0: cmac = &dev->bar2->cmac0; break;
            case 1: cmac = &dev->bar2->cmac1; break;
            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_ID;
                goto write_response;
            }

            switch (config.state()) {
            case PortState::PORT_STATE_UNKNOWN: // Field is unset.
                break;

            case PortState::PORT_STATE_DISABLE:
                cmac_disable(cmac);
                break;

            case PortState::PORT_STATE_ENABLE:
                cmac_enable(cmac);
                break;

            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_STATE;
                goto write_response;
            }

            switch (config.fec()) {
            case PortFec::PORT_FEC_UNKNOWN: // Field is unset.
                break;

            case PortFec::PORT_FEC_NONE:
                cmac_rsfec_disable(cmac);
                break;

            case PortFec::PORT_FEC_REED_SOLOMON:
                cmac_rsfec_enable(cmac);
                break;

            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_FEC;
                goto write_response;
            }

            switch (config.loopback()) {
            case PortLoopback::PORT_LOOPBACK_UNKNOWN: // Field is unset.
                break;

            case PortLoopback::PORT_LOOPBACK_NONE:
                cmac_loopback_disable(cmac);
                break;

            case PortLoopback::PORT_LOOPBACK_NEAR_END_PMA:
                cmac_loopback_enable(cmac);
                break;

            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_LOOPBACK;
                goto write_response;
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_port_config(
    const PortConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_port_config(req, [&rdwr](const PortConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_port_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetPortConfig(
    [[maybe_unused]] ServerContext* ctx,
    const PortConfigRequest* req,
    ServerWriter<PortConfigResponse>* writer) {
    set_port_config(*req, [&writer](const PortConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_port_status(
    const PortStatusRequest& req,
    function<void(const PortStatusResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortStatusResponse resp;
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

        int begin_port_id = 0;
        int end_port_id = dev->nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortStatusResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortStatusResponse resp;
            auto err = ErrorCode::EC_OK;

            volatile typeof(dev->bar2->cmac0)* cmac;
            switch (port_id) {
            case 0: cmac = &dev->bar2->cmac0; break;
            case 1: cmac = &dev->bar2->cmac1; break;
            default:
                err = ErrorCode::EC_UNSUPPORTED_PORT_ID;
                goto write_response;
            }

            {
                auto status = resp.mutable_status();
                status->set_link(cmac_rx_status_is_link_up(cmac) ?
                                 PortLink::PORT_LINK_UP :
                                 PortLink::PORT_LINK_DOWN);
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_port_status(
    const PortStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_port_status(req, [&rdwr](const PortStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_port_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetPortStatus(
    [[maybe_unused]] ServerContext* ctx,
    const PortStatusRequest* req,
    ServerWriter<PortStatusResponse>* writer) {
    get_port_status(*req, [&writer](const PortStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_or_clear_port_stats(
    const PortStatsRequest& req,
    bool do_clear,
    function<void(const PortStatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortStatsResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    GetStatsContext ctx;
    if (!do_clear) {
        auto filters = req.filters();
        ctx.non_zero = filters.non_zero();

        auto ntypes = filters.metric_types_size();
        if (ntypes > 0) {
            for (auto n = 0; n < ntypes; ++n) {
                ctx.metric_types.set(filters.metric_types(n));
            }
        } else {
            ctx.metric_types.set(StatsMetricType::STATS_METRIC_TYPE_COUNTER);
        }
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_port_id = 0;
        int end_port_id = dev->nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortStatsResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        auto& zones = dev->stats.zones[DeviceStatsZone::PORT_COUNTERS];
        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortStatsResponse resp;

            if (do_clear) {
                stats_zone_clear_metrics(zones[port_id]->zone);
            } else {
                ctx.stats = resp.mutable_stats();
                stats_zone_for_each_metric(zones[port_id]->zone, get_stats_for_each_metric, &ctx);
            }

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_port_stats(
    const PortStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_port_stats(req, false, [&rdwr](const PortStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_port_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetPortStats(
    [[maybe_unused]] ServerContext* ctx,
    const PortStatsRequest* req,
    ServerWriter<PortStatsResponse>* writer) {
    get_or_clear_port_stats(*req, false, [&writer](const PortStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_port_stats(
    const PortStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_port_stats(req, true, [&rdwr](const PortStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_port_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::ClearPortStats(
    [[maybe_unused]] ServerContext* ctx,
    const PortStatsRequest* req,
    ServerWriter<PortStatsResponse>* writer) {
    get_or_clear_port_stats(*req, true, [&writer](const PortStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
