#include "agent.hpp"

#include <iostream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_device_info(
    const DeviceInfoRequest& req,
    function<void(const DeviceInfoResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        DeviceInfoResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        DeviceInfoResponse resp;
        auto info = resp.mutable_info();

        auto pci = info->mutable_pci();
        pci->set_bus_id(devices[dev_id].bus_id);
        pci->set_vendor_id(0x1234);
        pci->set_device_id(dev_id);

        auto build = info->mutable_build();
        build->set_number(0x12345678);
        build->set_status(0x9abcdef0);
        for (auto i = 0; i < 3; ++i) {
            build->add_dna(i);
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_device_info(
    const DeviceInfoRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_device_info(req, [&rdwr](const DeviceInfoResponse& resp) -> void {
        BatchResponse bresp;
        auto info = bresp.mutable_device_info();
        info->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetDeviceInfo(
    [[maybe_unused]] ServerContext* ctx,
    const DeviceInfoRequest* req,
    ServerWriter<DeviceInfoResponse>* writer) {
    get_device_info(*req, [&writer](const DeviceInfoResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_device_status(
    const DeviceStatusRequest& req,
    function<void(const DeviceStatusResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        DeviceStatusResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        DeviceStatusResponse resp;
        auto status = resp.mutable_status();

        for (auto i = 0; i < 5; ++i) {
            auto sysmon = status->add_sysmons();
            sysmon->set_index(i);
            sysmon->set_temperature((i + 1) * 10.0);
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_device_status(
    const DeviceStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_device_status(req, [&rdwr](const DeviceStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_device_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetDeviceStatus(
    [[maybe_unused]] ServerContext* ctx,
    const DeviceStatusRequest* req,
    ServerWriter<DeviceStatusResponse>* writer) {
    get_device_status(*req, [&writer](const DeviceStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
