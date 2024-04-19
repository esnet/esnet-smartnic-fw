#include "agent.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "syscfg_block.h"
#include "sysmon.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
static uint16_t read_hex_pci_id(const string& bus_id, const string& file) {
    string path = "/sys/bus/pci/devices/" + bus_id + '/' + file;
    ifstream in(path, ios::in);
    if (!in.is_open()) {
        cerr << "ERROR: Failed to open file '" << path << "'." << endl;
        return 0xffff;
    }

    ostringstream out;
    out << in.rdbuf();
    if (in.fail()) {
        cerr << "ERROR: Failed to read file '" << path << "'." << endl;
        return 0xffff;
    }

    return stoi(out.str(), 0, 16);
}

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
        const auto dev = devices[dev_id];
        DeviceInfoResponse resp;
        auto info = resp.mutable_info();

        auto pci = info->mutable_pci();
        pci->set_bus_id(dev->bus_id);
        pci->set_vendor_id(read_hex_pci_id(dev->bus_id, "vendor"));
        pci->set_device_id(read_hex_pci_id(dev->bus_id, "device"));

        auto build = info->mutable_build();
        volatile auto syscfg = &dev->bar2->syscfg;

        build->set_number(syscfg->usr_access);
        build->set_status(syscfg->build_status);
        for (auto d = 0; d < SYSCFG_DNA_COUNT; ++d) {
            build->add_dna(syscfg->dna[d]);
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
        const auto dev = devices[dev_id];
        DeviceStatusResponse resp;
        auto status = resp.mutable_status();

        auto sysmon = status->add_sysmons();
        sysmon->set_index(0);
        sysmon->set_temperature(sysmon_get_temp(&dev->bar2->sysmon0));

        sysmon = status->add_sysmons();
        sysmon->set_index(1);
        sysmon->set_temperature(sysmon_get_temp(&dev->bar2->sysmon1));

        sysmon = status->add_sysmons();
        sysmon->set_index(2);
        sysmon->set_temperature(sysmon_get_temp(&dev->bar2->sysmon2));

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
