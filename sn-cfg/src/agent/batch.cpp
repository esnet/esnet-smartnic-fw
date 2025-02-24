#include "agent.hpp"

#include <grpc/grpc.h>
#include "sn_cfg_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_cfg::v2;

//--------------------------------------------------------------------------------------------------
static void error_resp(
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr,
    ErrorCode err,
    BatchOperation op) {
    BatchResponse resp;
    resp.set_error_code(err);
    resp.set_op(op);
    rdwr->Write(resp);
}

//--------------------------------------------------------------------------------------------------
static const char* item_case_name(const BatchRequest::ItemCase item_case) {
    switch (item_case) {
    case BatchRequest::ItemCase::kDeviceInfo: return "DeviceInfo";
    case BatchRequest::ItemCase::kDeviceStatus: return "DeviceStatus";
    case BatchRequest::ItemCase::kHostConfig: return "HostConfig";
    case BatchRequest::ItemCase::kHostStats: return "HostStats";
    case BatchRequest::ItemCase::kPortConfig: return "PortConfig";
    case BatchRequest::ItemCase::kPortStatus: return "PortStatus";
    case BatchRequest::ItemCase::kPortStats: return "PortStats";
    case BatchRequest::ItemCase::kSwitchConfig: return "SwitchConfig";
    case BatchRequest::ItemCase::kSwitchStats: return "SwitchStats";
    case BatchRequest::ItemCase::kDefaults: return "Defaults";
    case BatchRequest::ItemCase::kStats: return "Stats";
    case BatchRequest::ItemCase::kModuleInfo: return "ModuleInfo";
    case BatchRequest::ItemCase::kModuleStatus: return "ModuleStatus";
    case BatchRequest::ItemCase::kModuleMem: return "ModuleMem";
    case BatchRequest::ItemCase::kModuleGpio: return "ModuleGpio";
    case BatchRequest::ItemCase::kServerStatus: return "ServerStatus";
    case BatchRequest::ItemCase::kServerConfig: return "ServerConfig";
    case BatchRequest::ItemCase::ITEM_NOT_SET: return "ITEM_NOT_SET";
    }
    return "UNKNOWN";
}
//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::Batch(
    [[maybe_unused]] ServerContext* ctx,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    ServerDebugFlag debug_flag = ServerDebugFlag::DEBUG_FLAG_BATCH;
    while (true) {
        BatchRequest req;
        if (!rdwr->Read(&req)) {
            break;
        }

        auto op = req.op();
        auto item_case = req.item_case();
        SERVER_LOG_IF_DEBUG(debug_flag, INFO,
            "op=" << BatchOperation_Name(op) << " (" << op << "), "
            "item_case=" << item_case_name(item_case) << " (" << item_case << ")");

        switch (item_case) {
        case BatchRequest::ItemCase::kDefaults:
            switch (op) {
            case BatchOperation::BOP_SET:
                batch_set_defaults(req.defaults(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kDeviceInfo:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_device_info(req.device_info(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kDeviceStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_device_status(req.device_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
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
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kHostStats:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_host_stats(req.host_stats(), rdwr);
                break;

            case BatchOperation::BOP_CLEAR:
                batch_clear_host_stats(req.host_stats(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kModuleGpio:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_module_gpio(req.module_gpio(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_module_gpio(req.module_gpio(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kModuleInfo:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_module_info(req.module_info(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kModuleMem:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_module_mem(req.module_mem(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_module_mem(req.module_mem(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kModuleStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_module_status(req.module_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kPortConfig:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_port_config(req.port_config(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_port_config(req.port_config(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kPortStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_port_status(req.port_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kPortStats:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_port_stats(req.port_stats(), rdwr);
                break;

            case BatchOperation::BOP_CLEAR:
                batch_clear_port_stats(req.port_stats(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kSwitchConfig:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_switch_config(req.switch_config(), rdwr);
                break;

            case BatchOperation::BOP_SET:
                batch_set_switch_config(req.switch_config(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kSwitchStats:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_switch_stats(req.switch_stats(), rdwr);
                break;

            case BatchOperation::BOP_CLEAR:
                batch_clear_switch_stats(req.switch_stats(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
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
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        case BatchRequest::ItemCase::kServerStatus:
            switch (op) {
            case BatchOperation::BOP_GET:
                batch_get_server_status(req.server_status(), rdwr);
                break;

            default:
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
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
                error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_OP, op);
                break;
            }
            break;

        default:
            error_resp(rdwr, ErrorCode::EC_UNKNOWN_BATCH_REQUEST, op);
            break;
        }
    }

    return Status::OK;
}
