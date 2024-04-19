#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
struct GetStatsContext {
    Stats* stats;
};

extern "C" {
    static int __get_stats_counters(const struct stats_for_each_spec* spec,
                                    uint64_t value, void* arg) {
        GetStatsContext* ctx = static_cast<GetStatsContext*>(arg);

        auto cnt = ctx->stats->add_counters();
        cnt->set_domain(spec->domain->name);
        cnt->set_zone(spec->zone->name);
        cnt->set_block(spec->block->name);
        cnt->set_name(spec->counter->name);
        cnt->set_value(value);

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_stats(
    const StatsRequest& req,
    function<void(const StatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        StatsResponse resp;
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
        StatsResponse resp;
        GetStatsContext ctx = {
            .stats = resp.mutable_stats(),
        };

        stats_domain_for_each_counter(dev->stats.domain, __get_stats_counters, &ctx);

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_stats(req, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    get_stats(*req, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::clear_stats(
    const StatsRequest& req,
    function<void(const StatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        StatsResponse resp;
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
        stats_domain_clear_counters(dev->stats.domain);

        StatsResponse resp;
        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    clear_stats(req, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::ClearStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    clear_stats(*req, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
