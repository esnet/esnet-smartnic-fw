#include "agent.hpp"

#include <iostream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_port_config(
    const PortConfigRequest& req,
    function<void(const PortConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        int begin_port_id = 0;
        int end_port_id = devices[dev_id].nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortConfigResponse resp;

            auto config = resp.mutable_config();
            config->set_state(PortState::PORT_STATE_DISABLE);
            config->set_fec(PortFec::PORT_FEC_NONE);
            config->set_loopback(PortLoopback::PORT_LOOPBACK_NONE);

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_port_config(
    const PortConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_port_config(req, [&rdwr](const PortConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_port_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetPortConfig(
    [[maybe_unused]] ServerContext* ctx,
    const PortConfigRequest* req,
    ServerWriter<PortConfigResponse>* writer) {
    get_port_config(*req, [&writer](const PortConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_port_config(
    const PortConfigRequest& req,
    function<void(const PortConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        PortConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_PORT_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        int begin_port_id = 0;
        int end_port_id = devices[dev_id].nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortConfigResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortConfigResponse resp;

            cerr <<
                "----------------------------------------" << endl <<
                "--- " << __func__ << " Port ID: " << port_id <<
                " on device ID " << dev_id << endl;

            cerr <<
                "--- " << __func__ << endl <<
                "state = " << PortState_Name(config.state()) << endl <<
                "fec = " << PortFec_Name(config.fec()) << endl <<
                "loopback = " << PortLoopback_Name(config.loopback()) << endl;

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_port_config(
    const PortConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_port_config(req, [&rdwr](const PortConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_port_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetPortConfig(
    [[maybe_unused]] ServerContext* ctx,
    const PortConfigRequest* req,
    ServerWriter<PortConfigResponse>* writer) {
    set_port_config(*req, [&writer](const PortConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_port_status(
    const PortStatusRequest& req,
    function<void(const PortStatusResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        PortStatusResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        int begin_port_id = 0;
        int end_port_id = devices[dev_id].nports - 1;
        int port_id = req.port_id(); // 0-based index. -1 means all ports.
        if (port_id > end_port_id) {
            PortStatusResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PORT_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);
            continue;
        }

        if (port_id > -1) {
            begin_port_id = port_id;
            end_port_id = port_id;
        }

        for (port_id = begin_port_id; port_id <= end_port_id; ++port_id) {
            PortStatusResponse resp;

            auto status = resp.mutable_status();
            status->set_link(PortLink::PORT_LINK_DOWN);

            resp.set_error_code(ErrorCode::EC_OK);
            resp.set_dev_id(dev_id);
            resp.set_port_id(port_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_port_status(
    const PortStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_port_status(req, [&rdwr](const PortStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_port_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetPortStatus(
    [[maybe_unused]] ServerContext* ctx,
    const PortStatusRequest* req,
    ServerWriter<PortStatusResponse>* writer) {
    get_port_status(*req, [&writer](const PortStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
