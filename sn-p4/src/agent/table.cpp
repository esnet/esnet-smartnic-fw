#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>
#include <gmp.h>
#include <sstream>
#include <stdint.h>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::clear_table(
    const TableRequest& req,
    function<void(const TableResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        TableResponse resp;
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
            TableResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        const auto table_name = req.table_name();
        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            TableResponse resp;
            auto err = ErrorCode::EC_OK;

            // Request table_name empty means all tables in the pipeline.
            if (table_name.empty()) {
                if (!snp4_reset_all_tables(pipeline->handle)) {
                    err = ErrorCode::EC_FAILED_CLEAR_ALL_TABLES;
                }
            } else if (!pipeline_has_table(pipeline, table_name)) {
                err = ErrorCode::EC_INVALID_TABLE_NAME;
            } else if (!snp4_reset_one_table(pipeline->handle, table_name.c_str())) {
                err = ErrorCode::EC_FAILED_CLEAR_TABLE;
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_clear_table(
    const TableRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    clear_table(req, [&rdwr](const TableResponse& resp) -> void {
        BatchResponse bresp;
        auto table = bresp.mutable_table();
        table->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::ClearTable(
    [[maybe_unused]] ServerContext* ctx,
    const TableRequest* req,
    ServerWriter<TableResponse>* writer) {
    clear_table(*req, [&writer](const TableResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode table_rule_parse_match(const Match& match, struct sn_match& m) {
    auto err = ErrorCode::EC_OK;
    switch (match.type_case()) {
    case Match::kKeyMask: {
        const auto km = match.key_mask();

        m.t = SN_MATCH_FORMAT_KEY_MASK;
        if (mpz_init_set_str(m.v.key_mask.key, km.key().c_str(), 0) != 0) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_INVALID_KEY_FORMAT;
        } else if (mpz_init_set_str(m.v.key_mask.mask, km.mask().c_str(), 0) != 0) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_INVALID_MASK_FORMAT;
        }
        break;
    }

    case Match::kKeyOnly: {
        const auto ko = match.key_only();

        m.t = SN_MATCH_FORMAT_KEY_ONLY;
        if (mpz_init_set_str(m.v.key_only.key, ko.key().c_str(), 0) != 0) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_INVALID_KEY_FORMAT;
        }
        break;
    }

    case Match::kKeyPrefix: {
        const auto kp = match.key_prefix();

        auto length = kp.prefix_length();
        if (length > UINT16_MAX) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_INVALID_PREFIX_LENGTH;
            break;
        }
        m.v.prefix.prefix_len = length;

        m.t = SN_MATCH_FORMAT_PREFIX;
        if (mpz_init_set_str(m.v.prefix.key, kp.key().c_str(), 0) != 0) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_INVALID_KEY_FORMAT;
        }
        break;
    }

    case Match::kRange: {
        const auto range = match.range();

        auto lower = range.lower();
        if (lower > UINT16_MAX) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_RANGE_LOWER_TOO_BIG;
            break;
        }

        auto upper = range.upper();
        if (upper > UINT16_MAX) {
            err = ErrorCode::EC_TABLE_RULE_MATCH_RANGE_UPPER_TOO_BIG;
            break;
        }

        m.t = SN_MATCH_FORMAT_RANGE;
        m.v.range.lower = lower;
        m.v.range.upper = upper;
        break;
    }

    case Match::kUnused:
        m.t = SN_MATCH_FORMAT_UNUSED;
        break;

    default:
        err = ErrorCode::EC_UNKNOWN_TABLE_RULE_MATCH_TYPE;
        break;
    }

    return err;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode table_rule_parse_action_parameter(const ActionParameter& param,
                                                   struct sn_param& p) {
    p.t = SN_PARAM_FORMAT_MPZ;
    if (mpz_init_set_str(p.v.mpz, param.value().c_str(), 0) != 0) {
        return ErrorCode::EC_TABLE_RULE_INVALID_ACTION_PARAMETER_FORMAT;
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode table_rule_pack(const DevicePipeline* pipeline,
                                 const struct snp4_info_table* ti,
                                 const struct sn_rule* rule,
                                 struct sn_pack* pack) {
    enum snp4_status status;
    if (ti != NULL) {
        status = snp4_rule_pack_matches(
            ti->matches, ti->key_bits, rule->matches, rule->num_matches, pack);
    } else {
        status = snp4_rule_pack(&pipeline->info, rule, pack);
    }

    auto err = ErrorCode::EC_UNKNOWN;
    switch (status) {
    case SNP4_STATUS_OK:
        err = ErrorCode::EC_OK;
        break;

#define CASE_STATUS(_name) case SNP4_STATUS_##_name: err = ErrorCode::EC_TABLE_RULE_##_name; break;
    CASE_STATUS(FIELD_SPEC_OVERFLOW);
    CASE_STATUS(FIELD_SPEC_FORMAT_INVALID);
    CASE_STATUS(FIELD_SPEC_UNKNOWN_TYPE);
    CASE_STATUS(FIELD_SPEC_SIZE_MISMATCH);
    CASE_STATUS(PACK_KEY_TOO_BIG);
    CASE_STATUS(PACK_MASK_TOO_BIG);
    CASE_STATUS(PACK_PARAMS_TOO_BIG);
    CASE_STATUS(MATCH_INVALID_FORMAT);
    CASE_STATUS(MATCH_MASK_TOO_WIDE);
    CASE_STATUS(MATCH_INVALID_BITFIELD_MASK);
    CASE_STATUS(MATCH_INVALID_CONSTANT_MASK);
    CASE_STATUS(MATCH_INVALID_PREFIX_MASK);
    CASE_STATUS(MATCH_INVALID_RANGE_MASK);
    CASE_STATUS(MATCH_INVALID_UNUSED_MASK);
    CASE_STATUS(MATCH_KEY_TOO_BIG);
    CASE_STATUS(MATCH_MASK_TOO_BIG);
    CASE_STATUS(INVALID_TABLE_NAME);
    CASE_STATUS(INVALID_TABLE_CONFIG);
    CASE_STATUS(INVALID_ACTION_FOR_TABLE);
    CASE_STATUS(PARAM_INVALID_FORMAT);
    CASE_STATUS(PARAM_SPEC_OVERFLOW);
    CASE_STATUS(PARAM_SPEC_SIZE_MISMATCH);
    CASE_STATUS(PARAM_TOO_BIG);
#undef CASE_STATUS

    default:
        break;
    }

    return err;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::insert_or_delete_table_rule(
    const TableRuleRequest& req,
    bool do_insert,
    function<void(const TableRuleResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        TableRuleResponse resp;
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
            TableRuleResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        const auto rule = req.rule();
        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            TableRuleResponse resp;
            auto err = ErrorCode::EC_OK;

            const auto ti = pipeline_get_table_info(pipeline, rule.table_name());
            if (ti == NULL) {
                err = ErrorCode::EC_INVALID_TABLE_NAME;
                goto write_response;
            }

            struct sn_rule sr;
            snp4_rule_init(&sr);
            sr.table_name = ti->name;
            sr.priority = rule.priority();

            {
                // Matches used for both insert and delete operations.
                auto nmatches = rule.matches_size();
                if (nmatches < ti->num_matches) {
                    err = ErrorCode::EC_TABLE_RULE_TOO_FEW_MATCHES;
                    goto clear_rule;
                } else if (nmatches > ti->num_matches) {
                    err = ErrorCode::EC_TABLE_RULE_TOO_MANY_MATCHES;
                    goto clear_rule;
                }

                for (const auto& match : rule.matches()) {
                    err = table_rule_parse_match(match, sr.matches[sr.num_matches++]);
                    if (err != ErrorCode::EC_OK) {
                        goto clear_rule;
                    }
                }
            }

            if (do_insert) {
                // Actions used only for insert operation.
                const auto action = rule.action();
                const auto ai = pipeline_get_table_action_info(ti, action.name());
                if (ai == NULL) {
                    err = ErrorCode::EC_INVALID_ACTION_NAME;
                    goto clear_rule;
                }
                sr.action_name = ai->name;

                auto nparams = action.parameters_size();
                if (nparams < ai->num_params) {
                    err = ErrorCode::EC_TABLE_RULE_TOO_FEW_ACTION_PARAMETERS;
                    goto clear_rule;
                } else if (nparams > ai->num_params) {
                    err = ErrorCode::EC_TABLE_RULE_TOO_MANY_ACTION_PARAMETERS;
                    goto clear_rule;
                }

                for (const auto& param : action.parameters()) {
                    err = table_rule_parse_action_parameter(param, sr.params[sr.num_params++]);
                    if (err != ErrorCode::EC_OK) {
                        goto clear_rule;
                    }
                }
            }

            {
                struct sn_pack pack;
                snp4_pack_init(&pack);

                err = table_rule_pack(pipeline, do_insert ? NULL : ti, &sr, &pack);
                if (err != ErrorCode::EC_OK) {
                    goto clear_rule;
                }

                if (do_insert) {
                    if (!snp4_table_insert_kma(pipeline->handle,
                                               sr.table_name,
                                               pack.key, pack.key_len,
                                               pack.mask, pack.mask_len,
                                               sr.action_name,
                                               pack.params, pack.params_len,
                                               sr.priority,
                                               rule.replace())) {
                        err = ErrorCode::EC_FAILED_INSERT_TABLE_RULE;
                    }
                } else if (!snp4_table_delete_k(pipeline->handle,
                                                sr.table_name,
                                                pack.key, pack.key_len,
                                                pack.mask, pack.mask_len)) {
                    err = ErrorCode::EC_FAILED_DELETE_TABLE_RULE;
                }
                snp4_pack_clear(&pack);
            }

        clear_rule:
            snp4_rule_clear(&sr);
        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_insert_table_rule(
    const TableRuleRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    insert_or_delete_table_rule(req, true, [&rdwr](const TableRuleResponse& resp) -> void {
        BatchResponse bresp;
        auto rule = bresp.mutable_table_rule();
        rule->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_INSERT);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::InsertTableRule(
    [[maybe_unused]] ServerContext* ctx,
    const TableRuleRequest* req,
    ServerWriter<TableRuleResponse>* writer) {
    insert_or_delete_table_rule(*req, true, [&writer](const TableRuleResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_delete_table_rule(
    const TableRuleRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    insert_or_delete_table_rule(req, false, [&rdwr](const TableRuleResponse& resp) -> void {
        BatchResponse bresp;
        auto rule = bresp.mutable_table_rule();
        rule->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_DELETE);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::DeleteTableRule(
    [[maybe_unused]] ServerContext* ctx,
    const TableRuleRequest* req,
    ServerWriter<TableRuleResponse>* writer) {
    insert_or_delete_table_rule(*req, false, [&writer](const TableRuleResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
