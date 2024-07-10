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

        default:
            error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_REQUEST);
            break;
        }
    }

    return Status::OK;
}
