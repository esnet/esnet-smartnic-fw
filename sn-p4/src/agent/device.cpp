#include "agent.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "syscfg_block.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
const char* device_stats_domain_name(DeviceStatsDomain dom) {
    switch (dom) {
    case DeviceStatsDomain::COUNTERS: return "COUNTERS";

    case DeviceStatsDomain::NDOMAINS:
        break;
    }
    return "UNKNOWN";
}

//--------------------------------------------------------------------------------------------------
static uint16_t read_hex_pci_id(const string& bus_id, const string& file) {
    string path = "/sys/bus/pci/devices/" + bus_id + '/' + file;
    ifstream in(path, ios::in);
    if (!in.is_open()) {
        SERVER_LOG_LINE_INIT(device, ERROR, "Failed to open file '" << path << "'");
        return 0xffff;
    }

    ostringstream out;
    out << in.rdbuf();
    if (in.fail()) {
        SERVER_LOG_LINE_INIT(device, ERROR, "Failed to read file '" << path << "'");
        return 0xffff;
    }

    return stoi(out.str(), 0, 16);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_device_info(
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
        auto err = ErrorCode::EC_OK;
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

        auto hw_ver = getenv("SN_HW_VER");
        auto fw_ver = getenv("SN_FW_VER");
        auto sw_ver = getenv("SN_SW_VER");
        auto env = build->mutable_env();
        env->set_hw_version(hw_ver != NULL ? hw_ver : "NONE");
        env->set_fw_version(fw_ver != NULL ? fw_ver : "NONE");
        env->set_sw_version(sw_ver != NULL ? sw_ver : "NONE");

        resp.set_error_code(err);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_device_info(
    const DeviceInfoRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_device_info(req, [&rdwr](const DeviceInfoResponse& resp) -> void {
        BatchResponse bresp;
        auto info = bresp.mutable_device_info();
        info->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetDeviceInfo(
    [[maybe_unused]] ServerContext* ctx,
    const DeviceInfoRequest* req,
    ServerWriter<DeviceInfoResponse>* writer) {
    get_device_info(*req, [&writer](const DeviceInfoResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
