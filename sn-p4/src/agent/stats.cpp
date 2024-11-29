#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <bitset>
#include <cstdlib>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
extern "C" {
    int get_stats_for_each_metric(const struct stats_for_each_spec* spec) {
        GetStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);

        if (ctx->non_zero) {
            bool non_zero = false;
            for (auto v = spec->values; !non_zero && v < &spec->values[spec->nvalues]; ++v) {
                non_zero = v->u64 != 0;
            }

            if (!non_zero) {
                return 0;
            }
        }

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

        default:
            return 0;
        }

        if (!ctx->metric_types.test(type)) {
            return 0;
        }

        auto metric = ctx->stats->add_metrics();
        metric->set_type(type);
        metric->set_name(spec->metric->name);
        metric->set_num_elements(spec->metric->nelements);

        auto scope = metric->mutable_scope();
        scope->set_domain(spec->domain->name);
        scope->set_zone(spec->zone->name);
        scope->set_block(spec->block->name);

        auto last_update = metric->mutable_last_update();
        last_update->set_seconds(spec->last_update.tv_sec);
        last_update->set_nanos(spec->last_update.tv_nsec);

        for (unsigned int idx = 0; idx < spec->nvalues; ++idx) {
            auto v = &spec->values[idx];
            if (ctx->non_zero && v->u64 == 0) {
                continue;
            }

            auto value = metric->add_values();
            value->set_index(idx);
            value->set_u64(v->u64);
            value->set_f64(v->f64);
        }

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_or_clear_stats(
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
        ctx.non_zero = filters.non_zero();

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
void SmartnicP4Impl::batch_get_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, false, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    get_or_clear_stats(*req, false, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_clear_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, true, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::ClearStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    get_or_clear_stats(*req, true, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
