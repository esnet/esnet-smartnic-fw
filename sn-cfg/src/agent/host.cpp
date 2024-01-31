#include "agent.hpp"

#include <iostream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

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
        int begin_host_id = 0;
        int end_host_id = devices[dev_id].nhosts - 1;
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

            auto config = resp.mutable_config();
            auto dma = config->mutable_dma();
            dma->set_base_queue(128 * (host_id - 1));
            dma->set_num_queues(128);

            resp.set_error_code(ErrorCode::EC_OK);
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

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        int begin_host_id = 0;
        int end_host_id = devices[dev_id].nhosts - 1;
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

        auto dma = config.dma();
        for (host_id = begin_host_id; host_id <= end_host_id; ++host_id) {
            HostConfigResponse resp;

            cerr <<
                "----------------------------------------" << endl <<
                "--- " << __func__ << " Host ID: " << host_id <<
                " on device ID " << dev_id << endl;

            cerr <<
                "--- " << __func__ << endl <<
                "dma_base_queue = " << dma.base_queue() <<
                ", dma_num_queues = " << dma.num_queues() << endl;

            resp.set_error_code(ErrorCode::EC_OK);
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
