#include "agent.hpp"

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "qdma.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_host_config(
    const HostConfigRequest& req,
    function<void(const HostConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        HostConfigResponse resp;
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

        int begin_host_id = 0;
        int end_host_id = dev.nhosts - 1;
        int host_id = req.host_id(); // 0-based index. -1 means all host interfaces.
        if (host_id > end_host_id) {
            HostConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_HOST_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (host_id > -1) {
            begin_host_id = host_id;
            end_host_id = host_id;
        }

        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            volatile typeof(dev.bar2->qdma_func0)* qdma;
            switch (host_id) {
            case 0: qdma = &dev.bar2->qdma_func0; break;
            case 1: qdma = &dev.bar2->qdma_func1; break;
            default:
                err = ErrorCode::EC_UNSUPPORTED_HOST_ID;
                goto write_response;
            }

            unsigned int base_queue;
            unsigned int num_queues;
            if (!qdma_get_queues(qdma, &base_queue, &num_queues)) {
                err = ErrorCode::EC_FAILED_GET_DMA_QUEUES;
                goto write_response;
            }

            {
                auto config = resp.mutable_config();
                auto dma = config->mutable_dma();
                dma->set_base_queue(base_queue);
                dma->set_num_queues(num_queues);
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_host_id(host_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_host_config(
    const HostConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_host_config(req, [&rdwr](const HostConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_host_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetHostConfig(
    [[maybe_unused]] ServerContext* ctx,
    const HostConfigRequest* req,
    ServerWriter<HostConfigResponse>* writer) {
    get_host_config(*req, [&writer](const HostConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_host_config(
    const HostConfigRequest& req,
    function<void(const HostConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        HostConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        HostConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_HOST_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    if (!config.has_dma()) {
        HostConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_HOST_DMA_CONFIG);
        write_resp(resp);
        return;
    }
    auto dma = config.dma();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_host_id = 0;
        int end_host_id = dev.nhosts - 1;
        int host_id = req.host_id(); // 0-based index. -1 means all host interfaces.
        if (host_id > end_host_id) {
            HostConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_HOST_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (host_id > -1) {
            begin_host_id = host_id;
            end_host_id = host_id;
        }

        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostConfigResponse resp;
            auto err = ErrorCode::EC_OK;

            volatile typeof(dev.bar2->qdma_func0)* qdma;
            switch (host_id) {
            case 0: qdma = &dev.bar2->qdma_func0; break;
            case 1: qdma = &dev.bar2->qdma_func1; break;
            default:
                err = ErrorCode::EC_UNSUPPORTED_HOST_ID;
                goto write_response;
            }

            if (!qdma_set_queues(qdma, dma.base_queue(), dma.num_queues())) {
                err = ErrorCode::EC_FAILED_SET_DMA_QUEUES;
            }

        write_response:
            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_host_id(host_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_host_config(
    const HostConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_host_config(req, [&rdwr](const HostConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_host_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetHostConfig(
    [[maybe_unused]] ServerContext* ctx,
    const HostConfigRequest* req,
    ServerWriter<HostConfigResponse>* writer) {
    set_host_config(*req, [&writer](const HostConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
