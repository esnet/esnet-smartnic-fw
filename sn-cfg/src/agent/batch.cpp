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

        case BatchRequest::ItemCase::kDeviceStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_device_status(req.device_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP);
                break;
            }
            break;

        case BatchRequest::ItemCase::kHostConfig:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_host_config(req.host_config(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_host_config(req.host_config(), rdwr);
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
