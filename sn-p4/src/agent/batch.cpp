#include "agent.hpp"

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;

//--------------------------------------------------------------------------------------------------
static void error_resp(
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr,
    ErrorCode err) {
    BatchResponse resp;
    resp.set_error_code(err);
    rdwr->Write(resp);
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::Batch(
    [[maybe_unused]] ServerContext* ctx,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    while (true) {
        BatchRequest req;
        if (!rdwr->Read(&req)) {
            break;
        }

        auto op = req.op();
        switch (req.item_case()) {
        case BatchRequest::ItemCase::kDeviceInfo:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_device_info(req.device_info(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kPipelineInfo:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_pipeline_info(req.pipeline_info(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kPipelineStats:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_pipeline_stats(req.pipeline_stats(), rdwr);
                break;

            case BatchOperation::BOP_CLEAR:
                batch_clear_pipeline_stats(req.pipeline_stats(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kTable:
            switch (op) {
            case BatchOperation::BOP_CLEAR:
                batch_clear_table(req.table(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kTableRule:
            switch (op) {
            case BatchOperation::BOP_INSERT:
                batch_insert_table_rule(req.table_rule(), rdwr);
                break;

            case BatchOperation::BOP_DELETE:
                batch_delete_table_rule(req.table_rule(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kStats:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_stats(req.stats(), rdwr);
                break;

            case BatchOperation::BOP_CLEAR:
                batch_clear_stats(req.stats(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kServerStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_server_status(req.server_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kServerConfig:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_server_config(req.server_config(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_server_config(req.server_config(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        default:
            error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_REQUEST);
            break;
        }
    }

    return Status::OK;
}
