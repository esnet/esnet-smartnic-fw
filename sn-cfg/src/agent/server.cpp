#include "agent.hpp"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace sn_cfg::v1;
using namespace std;

//--------------------------------------------------------------------------------------------------
const char* SmartnicConfigImpl::debug_flag_label(const ServerDebugFlag flag) {
    switch (flag) {
    case ServerDebugFlag::DEBUG_FLAG_BATCH: return "BATCH";

    case ServerDebugFlag::DEBUG_FLAG_UNKNOWN:
    default:
        break;
    }
    return "UNKNOWN";
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_server(void) {
    auto rv = timespec_get(&timestamp.start_wall, TIME_UTC);
    if (rv != TIME_UTC) {
        cerr << "ERROR: timespec_get failed with " << rv << "." << endl;
        exit(EXIT_FAILURE);
    }

    rv = clock_gettime(CLOCK_MONOTONIC, &timestamp.start_mono);
    if (rv != 0) {
        cerr << "ERROR: monotonic clock_gettime failed with " << rv << "." << endl;
        exit(EXIT_FAILURE);
    }

    cout << "--- UTC start time: "
         << put_time(gmtime(&timestamp.start_wall.tv_sec), "%Y-%m-%d %H:%M:%S %z")
         << " ["
         << timestamp.start_wall.tv_sec << "s."
         << timestamp.start_wall.tv_nsec << "ns"
         << "]"
         << endl;

    // All stats domains are enabled by default.
    for (auto flag = ServerControlStatsFlag_MIN + 1; flag <= ServerControlStatsFlag_MAX; ++flag) {
        if (ServerControlStatsFlag_IsValid(flag)) {
            control.stats_flags.set(flag);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_server(void) {
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_server_status(
    [[maybe_unused]] const ServerStatusRequest& req,
    function<void(const ServerStatusResponse&)> write_resp) {
    ServerStatusResponse resp;
    auto err = ErrorCode::EC_OK;
    struct timespec now_mono;
    auto rv = clock_gettime(CLOCK_MONOTONIC, &now_mono);
    if (rv != 0) {
        err = ErrorCode::EC_SERVER_FAILED_GET_TIME;
    } else {
        auto status = resp.mutable_status();

        auto start_time = status->mutable_start_time();
        start_time->set_seconds(timestamp.start_wall.tv_sec);
        start_time->set_nanos(timestamp.start_wall.tv_nsec);

        struct timespec diff;
        diff.tv_sec = now_mono.tv_sec - timestamp.start_mono.tv_sec;
        diff.tv_nsec = now_mono.tv_nsec - timestamp.start_mono.tv_nsec;
        if (diff.tv_nsec < 0) {
            diff.tv_sec -= 1;
            diff.tv_nsec += 1000000000;
        }

        auto up_time = status->mutable_up_time();
        up_time->set_seconds(diff.tv_sec);
        up_time->set_nanos(diff.tv_nsec);
    }

    resp.set_error_code(err);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_server_status(
    const ServerStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_server_status(req, [&rdwr](const ServerStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_server_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetServerStatus(
    [[maybe_unused]] ServerContext* ctx,
    const ServerStatusRequest* req,
    ServerWriter<ServerStatusResponse>* writer) {
    get_server_status(*req, [&writer](const ServerStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_server_config(
    [[maybe_unused]] const ServerConfigRequest& req,
    function<void(const ServerConfigResponse&)> write_resp) {
    ServerConfigResponse resp;
    auto config = resp.mutable_config();

    auto dbg = config->mutable_debug();
    for (auto flag = ServerDebugFlag_MIN + 1; flag <= ServerDebugFlag_MAX; ++flag) {
        if (!ServerDebugFlag_IsValid(flag)) {
            continue;
        }

        if (debug.flags.test(flag)) {
            dbg->add_enables((ServerDebugFlag)flag);
        } else {
            dbg->add_disables((ServerDebugFlag)flag);
        }
    }

    auto ctrl = config->mutable_control();
    auto stats = ctrl->mutable_stats();
    for (auto flag = ServerControlStatsFlag_MIN + 1; flag <= ServerControlStatsFlag_MAX; ++flag) {
        if (!ServerControlStatsFlag_IsValid(flag)) {
            continue;
        }

        if (control.stats_flags.test(flag)) {
            stats->add_enables((ServerControlStatsFlag)flag);
        } else {
            stats->add_disables((ServerControlStatsFlag)flag);
        }
    }

    resp.set_error_code(ErrorCode::EC_OK);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_server_config(
    const ServerConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_server_config(req, [&rdwr](const ServerConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_server_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetServerConfig(
    [[maybe_unused]] ServerContext* ctx,
    const ServerConfigRequest* req,
    ServerWriter<ServerConfigResponse>* writer) {
    get_server_config(*req, [&writer](const ServerConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
ErrorCode SmartnicConfigImpl::apply_server_control_stats_flag(
   ServerControlStatsFlag flag,
   bool enable) {
    auto domain = DeviceStatsDomain::NDOMAINS;
    auto zone = DeviceStatsZone::NZONES;
    switch (flag) {
#define CASE_DOMAIN_FLAG(_flag) \
    case ServerControlStatsFlag::CTRL_STATS_FLAG_DOMAIN_##_flag: \
        domain = DeviceStatsDomain::_flag; \
        break
#define CASE_ZONE_FLAG(_flag) \
    case ServerControlStatsFlag::CTRL_STATS_FLAG_ZONE_##_flag: \
        zone = DeviceStatsZone::_flag; \
        break

    CASE_DOMAIN_FLAG(COUNTERS);
    CASE_DOMAIN_FLAG(MONITORS);
    CASE_DOMAIN_FLAG(MODULES);

    CASE_ZONE_FLAG(CARD_MONITORS);
    CASE_ZONE_FLAG(SYSMON_MONITORS);
    CASE_ZONE_FLAG(HOST_COUNTERS);
    CASE_ZONE_FLAG(PORT_COUNTERS);
    CASE_ZONE_FLAG(SWITCH_COUNTERS);
    CASE_ZONE_FLAG(MODULE_MONITORS);

    default:
        return ErrorCode::EC_SERVER_INVALID_CONTROL_STATS_FLAG;

#undef CASE_DOMAIN_FLAG
#undef CASE_ZONE_FLAG
    }

    if (enable) {
        control.stats_flags.set(flag);
    } else {
        control.stats_flags.reset(flag);
    }

    cout << (enable ? "En" : "Dis")
         << "abling collection for statistics '"
         << ServerControlStatsFlag_Name(flag)
         << "' (" << flag << ")."
         << endl;

    if (domain != DeviceStatsDomain::NDOMAINS) {
        for (auto dev : devices) {
            auto dom = dev->stats.domains[domain];
            if (enable) {
                stats_domain_start(dom);
            } else {
                stats_domain_stop(dom);
            }
        }
    } else {
        for (auto dev : devices) {
            for (auto zn : dev->stats.zones[zone]) {
                if (enable) {
                    stats_zone_enable(zn->zone);
                } else {
                    stats_zone_disable(zn->zone);
                }
            }
        }
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_server_config(
    const ServerConfigRequest& req,
    function<void(const ServerConfigResponse&)> write_resp) {
    ServerConfigResponse resp;
    auto err = ErrorCode::EC_OK;

    auto config = req.config();
    if (config.has_debug()) {
        auto dbg = config.debug();

        for (auto flag : dbg.enables()) {
            if (!ServerDebugFlag_IsValid(flag)) {
                err = ErrorCode::EC_SERVER_INVALID_DEBUG_FLAG;
                goto write_response;
            }
            debug.flags.set(flag);
        }

        for (auto flag : dbg.disables()) {
            if (!ServerDebugFlag_IsValid(flag)) {
                err = ErrorCode::EC_SERVER_INVALID_DEBUG_FLAG;
                goto write_response;
            }
            debug.flags.reset(flag);
        }
    }

    if (config.has_control()) {
        auto ctrl = config.control();
        if (ctrl.has_stats()) {
            auto stats = ctrl.stats();
            for (auto flag : stats.enables()) {
                err = apply_server_control_stats_flag((ServerControlStatsFlag)flag, true);
                if (err != ErrorCode::EC_OK) {
                    goto write_response;
                }
            }

            for (auto flag : stats.disables()) {
                err = apply_server_control_stats_flag((ServerControlStatsFlag)flag, false);
                if (err != ErrorCode::EC_OK) {
                    goto write_response;
                }
            }
        }
    }

 write_response:
    resp.set_error_code(err);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_server_config(
    const ServerConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_server_config(req, [&rdwr](const ServerConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_server_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetServerConfig(
    [[maybe_unused]] ServerContext* ctx,
    const ServerConfigRequest* req,
    ServerWriter<ServerConfigResponse>* writer) {
    set_server_config(*req, [&writer](const ServerConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
