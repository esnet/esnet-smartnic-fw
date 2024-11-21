#include "agent.hpp"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
const char* SmartnicP4Impl::debug_flag_label(const ServerDebugFlag flag) {
    switch (flag) {
    case ServerDebugFlag::DEBUG_FLAG_TABLE_CLEAR: return "TABLE_CLEAR";
    case ServerDebugFlag::DEBUG_FLAG_TABLE_RULE_INSERT: return "TABLE_RULE_INSERT";
    case ServerDebugFlag::DEBUG_FLAG_TABLE_RULE_DELETE: return "TABLE_RULE_DELETE";
    case ServerDebugFlag::DEBUG_FLAG_PIPELINE_INFO: return "PIPELINE_INFO";

    case ServerDebugFlag::DEBUG_FLAG_UNKNOWN:
    default:
        break;
    }
    return "UNKNOWN";
}

//--------------------------------------------------------------------------------------------------
bool SmartnicP4Impl::get_server_times(struct timespec* start, struct timespec* up) {
    if (start != NULL) {
        start->tv_sec = timestamp.start_wall.tv_sec;
        start->tv_nsec = timestamp.start_wall.tv_nsec;
    }

    if (up != NULL) {
        struct timespec now_mono;
        auto rv = clock_gettime(CLOCK_MONOTONIC, &now_mono);
        if (rv != 0) {
            return false;
        }

        up->tv_sec = now_mono.tv_sec - timestamp.start_mono.tv_sec;
        up->tv_nsec = now_mono.tv_nsec - timestamp.start_mono.tv_nsec;
        if (up->tv_nsec < 0) {
            up->tv_sec -= 1;
            up->tv_nsec += 1000000000;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
extern "C" {
    enum ServerStat {
        START_TIME,
        UP_TIME,
    };

    static void server_stats_read_metric(const struct stats_block_spec* bspec,
                                         const struct stats_metric_spec* mspec,
                                         uint64_t* value,
                                         [[maybe_unused]] void* data) {
        SmartnicP4Impl* impl = (typeof(impl))bspec->io.data.ptr;

        struct timespec start;
        struct timespec up;
        if (!impl->get_server_times(&start, &up)) {
            return;
        }

        switch ((ServerStat)mspec->io.data.u64) {
        case ServerStat::START_TIME:
            *value = start.tv_sec;
            break;

        case ServerStat::UP_TIME:
            *value = up.tv_sec;
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::init_server_stats(void) {
    struct stats_metric_spec mspecs[2];
    memset(mspecs, 0, sizeof(mspecs));
    auto mspec = mspecs;

    mspec->name = "start_time_sec";
    mspec->type = stats_metric_type_GAUGE;
    mspec->flags = STATS_METRIC_FLAG_MASK(NEVER_CLEAR);
    mspec->io.data.u64 = ServerStat::START_TIME;
    mspec += 1;

    mspec->name = "up_time_sec";
    mspec->type = stats_metric_type_GAUGE;
    mspec->flags = STATS_METRIC_FLAG_MASK(NEVER_CLEAR);
    mspec->io.data.u64 = ServerStat::UP_TIME;
    mspec += 1;

    struct stats_block_spec bspec;
    memset(&bspec, 0, sizeof(bspec));
    bspec.name = "status";
    bspec.metrics = mspecs;
    bspec.nmetrics = sizeof(mspecs) / sizeof(mspecs[0]);
    bspec.read_metric = server_stats_read_metric;
    bspec.io.data.ptr = this;

    struct stats_zone_spec zspec;
    memset(&zspec, 0, sizeof(zspec));
    zspec.name = "server";
    zspec.blocks = &bspec;
    zspec.nblocks = 1;

    auto stats = new ServerStats;
    stats->zone = stats_zone_alloc(server_stats.domain, &zspec);
    if (stats->zone == NULL) {
        cerr << "ERROR: Failed to alloc server status stats zone."  << endl;
        exit(EXIT_FAILURE);
    }

    server_stats.status = stats;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::init_server(void) {
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

    init_server_stats();

    cout << "--- UTC start time: "
         << put_time(gmtime(&timestamp.start_wall.tv_sec), "%Y-%m-%d %H:%M:%S %z")
         << " ["
         << timestamp.start_wall.tv_sec << "s."
         << timestamp.start_wall.tv_nsec << "ns"
         << "]"
         << endl;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::deinit_server(void) {
    auto stats = server_stats.status;
    server_stats.status = NULL;

    stats_zone_free(stats->zone);
    delete stats;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_server_status(
    [[maybe_unused]] const ServerStatusRequest& req,
    function<void(const ServerStatusResponse&)> write_resp) {
    ServerStatusResponse resp;
    auto err = ErrorCode::EC_OK;
    struct timespec start;
    struct timespec up;

    if (!get_server_times(&start, &up)) {
        err = ErrorCode::EC_SERVER_FAILED_GET_TIME;
    } else {
        auto status = resp.mutable_status();

        auto start_time = status->mutable_start_time();
        start_time->set_seconds(start.tv_sec);
        start_time->set_nanos(start.tv_nsec);

        auto up_time = status->mutable_up_time();
        up_time->set_seconds(up.tv_sec);
        up_time->set_nanos(up.tv_nsec);
    }

    resp.set_error_code(err);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_server_status(
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
Status SmartnicP4Impl::GetServerStatus(
    [[maybe_unused]] ServerContext* ctx,
    const ServerStatusRequest* req,
    ServerWriter<ServerStatusResponse>* writer) {
    get_server_status(*req, [&writer](const ServerStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_server_config(
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

    resp.set_error_code(ErrorCode::EC_OK);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_server_config(
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
Status SmartnicP4Impl::GetServerConfig(
    [[maybe_unused]] ServerContext* ctx,
    const ServerConfigRequest* req,
    ServerWriter<ServerConfigResponse>* writer) {
    get_server_config(*req, [&writer](const ServerConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::set_server_config(
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

 write_response:
    resp.set_error_code(err);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_set_server_config(
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
Status SmartnicP4Impl::SetServerConfig(
    [[maybe_unused]] ServerContext* ctx,
    const ServerConfigRequest* req,
    ServerWriter<ServerConfigResponse>* writer) {
    set_server_config(*req, [&writer](const ServerConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
