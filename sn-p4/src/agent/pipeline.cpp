#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

#include "snp4.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
const struct snp4_info_table*
SmartnicP4Impl::pipeline_get_table_info(const DevicePipeline* pipeline,
                                        const string& table_name) {
    const auto pi = &pipeline->info;
    for (auto idx = 0; idx < pi->num_tables; ++idx) {
        const auto ti = &pi->tables[idx];
        if (ti->name == table_name) {
            return ti;
        }
    }

    return NULL;
}

bool SmartnicP4Impl::pipeline_has_table(const DevicePipeline* pipeline, const string& table_name) {
    return pipeline_get_table_info(pipeline, table_name) != NULL;
}

//--------------------------------------------------------------------------------------------------
const struct snp4_info_action*
SmartnicP4Impl::pipeline_get_table_action_info(const struct snp4_info_table* ti,
                                               const string& action_name) {
    for (auto idx = 0; idx < ti->num_actions; ++idx) {
        const auto ai = &ti->actions[idx];
        if (ai->name == action_name) {
            return ai;
        }
    }

    return NULL;
}

bool SmartnicP4Impl::pipeline_has_table_action(const struct snp4_info_table* ti,
                                               const string& action_name) {
    return pipeline_get_table_action_info(ti, action_name) != NULL;
}

//--------------------------------------------------------------------------------------------------
const struct snp4_info_counter_block*
SmartnicP4Impl::pipeline_get_counter_block_info(const DevicePipeline* pipeline,
                                                const string& block_name) {
    const auto pi = &pipeline->info;
    for (auto idx = 0; idx < pi->num_counter_blocks; ++idx) {
        const auto bi = &pi->counter_blocks[idx];
        if (bi->name == block_name) {
            return bi;
        }
    }

    return NULL;
}

bool SmartnicP4Impl::pipeline_has_counter_block(const DevicePipeline* pipeline,
                                                const string& block_name) {
    return pipeline_get_counter_block_info(pipeline, block_name) != NULL;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::init_pipeline(Device* dev) {
    for (unsigned int id = 0; id < snp4_sdnet_count(); ++id) {
        if (!snp4_sdnet_present(id)) {
            continue;
        }
        SERVER_LOG_LINE_INIT(pipeline, INFO,
            "Initializing pipeline ID " << id << " on device " << dev->bus_id);

        auto pipeline = new DevicePipeline{
            .id = id,
            .handle = NULL,
            .info = {},
            .stats = {},
        };

        pipeline->handle = snp4_init(id, (uintptr_t)dev->bar2);
        if (pipeline->handle == NULL) {
            SERVER_LOG_LINE_INIT(pipeline, ERROR,
                "Failed to initialize snp4/vitisnetp4 library for pipeline ID " << id <<
                " on device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        SERVER_LOG_LINE_INIT(pipeline, INFO, "Resetting all tables of pipeline ID " << id);
        if (!snp4_reset_all_tables(pipeline->handle)) {
            SERVER_LOG_LINE_INIT(pipeline, ERROR,
                "Failed to reset snp4/vitisnetp4 tables of pipeline ID " << id <<
                " on device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        SERVER_LOG_LINE_INIT(pipeline, INFO,
            "Loading info of pipeline ID " << id << " on device " << dev->bus_id);
        auto rc = snp4_info_get_pipeline(id, &pipeline->info);
        if (rc != SNP4_STATUS_OK) {
            SERVER_LOG_LINE_INIT(pipeline, ERROR,
                "Failed to load snp4 info (" << rc << ") for pipeline ID " << id <<
                " on device " << dev->bus_id);
            exit(EXIT_FAILURE);
        }

        init_counters(dev, pipeline);
        init_table_ecc(dev, pipeline);

        dev->pipelines.push_back(pipeline);
        SERVER_LOG_LINE_INIT(pipeline, INFO,
            "Completed init of pipeline ID " << id << " on device " << dev->bus_id);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::deinit_pipeline(Device* dev) {
    while (!dev->pipelines.empty()) {
        auto pipeline = dev->pipelines.back();

        deinit_counters(pipeline);
        deinit_table_ecc(pipeline);

        if (!snp4_deinit(pipeline->handle)) {
            SERVER_LOG_LINE_INIT(pipeline, ERROR,
                "Failed to deinit snp4/vitisnetp4 library for pipeline ID " << pipeline->id <<
                " on device " << dev->bus_id);
        }

        dev->pipelines.pop_back();
        delete pipeline;
    }
}

//--------------------------------------------------------------------------------------------------
extern "C" {
    struct GetPipelinInfoDumpTableContext {
        const char* label;
    };

    bool get_pipeline_info_dump_table(const struct snp4_table_entry * entry, void * arg) {
        GetPipelinInfoDumpTableContext* ctx = static_cast<typeof(ctx)>(arg);

        ostringstream key;
        key << "0x" << setfill('0') << hex;
        for (unsigned int i = 0; i < entry->key.len; ++i) {
            key << setw(2) << (unsigned int)entry->key.value[i];
        }

        ostringstream mask;
        mask << "0x" << setfill('0') << hex;
        for (unsigned int i = 0; i < entry->key.len; ++i) {
            mask << setw(2) << (unsigned int)entry->key.mask[i];
        }

        ostringstream params;
        params << "0x" << setfill('0') << hex;
        for (unsigned int i = 0; i < entry->action.params.len; ++i) {
            params << setw(2) << (unsigned int)entry->action.params.value[i];
        }

        SERVER_LOG_DEBUG(ctx->label, INFO, "--> Key:      " << key.str() << endl);
        SERVER_LOG_DEBUG(ctx->label, INFO, "    Mask:     " << mask.str() << endl);
        SERVER_LOG_DEBUG(ctx->label, INFO, "    Priority: " << entry->priority << endl);
        SERVER_LOG_DEBUG(ctx->label, INFO, "    Action:   " << entry->action.name << endl);
        SERVER_LOG_DEBUG(ctx->label, INFO, "    Params:   " << params.str() << endl);

        return true;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_pipeline_info(
    const PipelineInfoRequest& req,
    function<void(const PipelineInfoResponse&)> write_resp) {
    auto debug_flag = ServerDebugFlag::DEBUG_FLAG_PIPELINE_INFO;
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PipelineInfoResponse resp;
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

        int begin_pipeline_id = 0;
        int end_pipeline_id = dev->pipelines.size() - 1;
        int pipeline_id = req.pipeline_id(); // 0-based index. -1 means all pipelines.
        if (pipeline_id > end_pipeline_id) {
            PipelineInfoResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            const auto pi = &pipeline->info;
            PipelineInfoResponse resp;
            auto err = ErrorCode::EC_OK;

            auto info = resp.mutable_info();
            info->set_name(pi->name);

            for (auto tidx = 0; tidx < pi->num_tables; ++tidx) {
                const auto ti = &pi->tables[tidx];
                auto table = info->add_tables();
                table->set_name(ti->name);
                table->set_num_entries(ti->num_entries);

                auto endian = TableEndian::TABLE_ENDIAN_UNKNOWN;
                switch (ti->endian) {
                case SNP4_INFO_TABLE_ENDIAN_LITTLE:
                    endian = TableEndian::TABLE_ENDIAN_LITTLE;
                    break;

                case SNP4_INFO_TABLE_ENDIAN_BIG:
                    endian = TableEndian::TABLE_ENDIAN_BIG;
                    break;
                }
                table->set_endian(endian);

                auto mode = TableMode::TABLE_MODE_UNKNOWN;
                switch (ti->mode) {
                case SNP4_INFO_TABLE_MODE_BCAM:
                    mode = TableMode::TABLE_MODE_BCAM;
                    break;

                case SNP4_INFO_TABLE_MODE_STCAM:
                    mode = TableMode::TABLE_MODE_STCAM;
                    table->set_num_masks(ti->num_masks);
                    break;

                case SNP4_INFO_TABLE_MODE_TCAM:
                    mode = TableMode::TABLE_MODE_TCAM;
                    break;

                case SNP4_INFO_TABLE_MODE_DCAM:
                    mode = TableMode::TABLE_MODE_DCAM;
                    break;

                case SNP4_INFO_TABLE_MODE_TINY_BCAM:
                    mode = TableMode::TABLE_MODE_TINY_BCAM;
                    break;

                case SNP4_INFO_TABLE_MODE_TINY_TCAM:
                    mode = TableMode::TABLE_MODE_TINY_TCAM;
                    break;
                }
                table->set_mode(mode);

                table->set_priority_required(ti->priority_required);
                table->set_key_width(ti->key_bits);
                table->set_response_width(ti->response_bits);
                table->set_priority_width(ti->priority_bits);
                table->set_action_id_width(ti->actionid_bits);

                for (auto midx = 0; midx < ti->num_matches; ++midx) {
                    const auto mi = &ti->matches[midx];
                    auto match = table->add_matches();
                    match->set_width(mi->bits);

                    auto type = MatchType::MATCH_TYPE_UNKNOWN;
                    switch (mi->type) {
                    case SNP4_INFO_MATCH_TYPE_INVALID:
                        break;

                    case SNP4_INFO_MATCH_TYPE_BITFIELD:
                        type = MatchType::MATCH_TYPE_BITFIELD;
                        break;

                    case SNP4_INFO_MATCH_TYPE_CONSTANT:
                        type = MatchType::MATCH_TYPE_CONSTANT;
                        break;

                    case SNP4_INFO_MATCH_TYPE_PREFIX:
                        type = MatchType::MATCH_TYPE_PREFIX;
                        break;

                    case SNP4_INFO_MATCH_TYPE_RANGE:
                        type = MatchType::MATCH_TYPE_RANGE;
                        break;

                    case SNP4_INFO_MATCH_TYPE_TERNARY:
                        type = MatchType::MATCH_TYPE_TERNARY;
                        break;

                    case SNP4_INFO_MATCH_TYPE_UNUSED:
                        type = MatchType::MATCH_TYPE_UNUSED;
                        break;

                    }
                    match->set_type(type);
                }

                for (auto aidx = 0; aidx < ti->num_actions; ++aidx) {
                    const auto ai = &ti->actions[aidx];
                    auto action = table->add_actions();

                    action->set_name(ai->name);
                    action->set_width(ai->param_bits);

                    for (auto pidx = 0; pidx < ai->num_params; ++pidx) {
                        const auto pi = &ai->params[pidx];
                        auto param = action->add_parameters();

                        param->set_name(pi->name);
                        param->set_width(pi->bits);
                    }
                }

                if (debug.flags.test(debug_flag)) {
                    GetPipelinInfoDumpTableContext ctx = {
                        .label = debug_flag_label(debug_flag),
                    };

                    SERVER_LOG_LINE_DEBUG(debug_flag, INFO, string(40, '-'));
                    SERVER_LOG_LINE_DEBUG(debug_flag, INFO, "Dump of table '" << ti->name << "':");
                    snp4_table_for_each_entry(pipeline->handle, ti->name,
                                              get_pipeline_info_dump_table, &ctx);

                    if (tidx == pi->num_tables - 1) {
                        SERVER_LOG_LINE_DEBUG(debug_flag, INFO, string(40, '-'));
                    }
                }
            }

            for (auto bidx = 0; bidx < pi->num_counter_blocks; ++bidx) {
                const auto bi = &pi->counter_blocks[bidx];
                auto block = info->add_counter_blocks();

                block->set_name(bi->name);
                block->set_width(bi->width);
                block->set_num_counters(bi->num_counters);

                auto type = CounterType::COUNTER_TYPE_UNKNOWN;
                switch (bi->type) {
                case SNP4_INFO_COUNTER_TYPE_PACKETS:
                    type = CounterType::COUNTER_TYPE_PACKETS;
                    break;

                case SNP4_INFO_COUNTER_TYPE_BYTES:
                    type = CounterType::COUNTER_TYPE_BYTES;
                    break;

                case SNP4_INFO_COUNTER_TYPE_PACKETS_AND_BYTES:
                    type = CounterType::COUNTER_TYPE_PACKETS_AND_BYTES;
                    break;

                case SNP4_INFO_COUNTER_TYPE_FLAG:
                    type = CounterType::COUNTER_TYPE_FLAG;
                    break;
                }
                block->set_type(type);
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_pipeline_info(
    const PipelineInfoRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_pipeline_info(req, [&rdwr](const PipelineInfoResponse& resp) -> void {
        BatchResponse bresp;
        auto info = bresp.mutable_pipeline_info();
        info->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetPipelineInfo(
    [[maybe_unused]] ServerContext* ctx,
    const PipelineInfoRequest* req,
    ServerWriter<PipelineInfoResponse>* writer) {
    get_pipeline_info(*req, [&writer](const PipelineInfoResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_or_clear_pipeline_stats(
    const PipelineStatsRequest& req,
    bool do_clear,
    function<void(const PipelineStatsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PipelineStatsResponse resp;
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

        int begin_pipeline_id = 0;
        int end_pipeline_id = dev->pipelines.size() - 1;
        int pipeline_id = req.pipeline_id(); // 0-based index. -1 means all pipelines.
        if (pipeline_id > end_pipeline_id) {
            PipelineStatsResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            auto pipeline = dev->pipelines[pipeline_id];
            PipelineStatsResponse resp;

            if (pipeline->stats.counters != NULL) {
                if (do_clear) {
                    stats_zone_clear_metrics(pipeline->stats.counters->zone, NULL);
                } else {
                    ctx.stats = resp.mutable_stats();
                    stats_zone_for_each_metric(
                        pipeline->stats.counters->zone, get_stats_for_each_metric, &ctx);
                }
            }

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_pipeline_stats(
    const PipelineStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_pipeline_stats(req, false, [&rdwr](const PipelineStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_pipeline_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetPipelineStats(
    [[maybe_unused]] ServerContext* ctx,
    const PipelineStatsRequest* req,
    ServerWriter<PipelineStatsResponse>* writer) {
    get_or_clear_pipeline_stats(*req, false, [&writer](const PipelineStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_clear_pipeline_stats(
    const PipelineStatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_pipeline_stats(req, true, [&rdwr](const PipelineStatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_pipeline_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::ClearPipelineStats(
    [[maybe_unused]] ServerContext* ctx,
    const PipelineStatsRequest* req,
    ServerWriter<PipelineStatsResponse>* writer) {
    get_or_clear_pipeline_stats(*req, true, [&writer](const PipelineStatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
