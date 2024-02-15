#include "agent.hpp"

#include <iostream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;

//--------------------------------------------------------------------------------------------------
static void error_resp(
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr,
    ErrorCode err) {
    BatchResponse resp;
    resp.set_error_code(err);
    rdwr->Write(resp);
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::Batch(
    [[maybe_unused]] ServerContext* ctx,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    while (true) {
        BatchRequest req;
        if (!rdwr->Read(&req)) {
            break;
        }

        auto op = req.op();
        switch (req.item_case()) {
        case BatchRequest::ItemCase::kDummy:
            switch (op) {
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
