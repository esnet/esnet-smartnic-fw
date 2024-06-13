#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <bitset>
#include <cstdlib>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
extern "C" {
    int get_stats_for_each_metric(const struct stats_for_each_spec* spec) {
        StatsMetricType type;
        switch (spec->metric->type) {
        case stats_metric_type_COUNTER:
            type = StatsMetricType::STATS_METRIC_TYPE_COUNTER;
            break;

        case stats_metric_type_GAUGE:
            type = StatsMetricType::STATS_METRIC_TYPE_GAUGE;
            break;

        case stats_metric_type_FLAG:
            type = StatsMetricType::STATS_METRIC_TYPE_FLAG;
            break;
        }

        GetStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);
        if (!ctx->metric_types.test(type)) {
            return 0;
        }

        auto metric = ctx->stats->add_metrics();
        metric->set_type(type);
        metric->set_name(spec->metric->name);

        auto scope = metric->mutable_scope();
        scope->set_domain(spec->domain->name);
        scope->set_zone(spec->zone->name);
        scope->set_block(spec->block->name);

        auto value = metric->mutable_value();
        value->set_u64(spec->value.u64);
        value->set_f64(spec->value.f64);

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_or_clear_stats(
    const StatsRequest& req,
    bool do_clear,
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

    GetStatsContext ctx;
    if (!do_clear) {
        auto filters = req.filters();
        auto ntypes = filters.metric_types_size();
        if (ntypes > 0) {
            for (auto n = 0; n < ntypes; ++n) {
                ctx.metric_types.set(filters.metric_types(n));
            }
        } else {
            ctx.metric_types.set(); // Set all bits.
        }
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        StatsResponse resp;
        if (!do_clear) {
            ctx.stats = resp.mutable_stats();
        }

        for (auto domain : dev->stats.domains) {
            if (do_clear) {
                stats_domain_clear_metrics(domain);
            } else {
                stats_domain_for_each_metric(domain, get_stats_for_each_metric, &ctx);
            }
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, false, [&rdwr](const StatsResponse& resp) -> void {
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
    get_or_clear_stats(*req, false, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_clear_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, true, [&rdwr](const StatsResponse& resp) -> void {
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
    get_or_clear_stats(*req, true, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
