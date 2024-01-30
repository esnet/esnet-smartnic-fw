#include "agent.hpp"

#include <iostream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_switch_config(
    const SwitchConfigRequest& req,
    function<void(const SwitchConfigResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        SwitchConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        SwitchConfigResponse resp;
        const auto dev = devices[dev_id];

        auto config = resp.mutable_config();
        for (unsigned int index = 0; index < dev.nports; ++index) {
            {
                auto src = config->add_ingress_sources();

                auto from_intf = src->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_PORT);
                from_intf->set_index(index);

                auto to_intf = src->mutable_to_intf();
                to_intf->set_itype(SwitchInterfaceType::SW_INTF_PORT);
                to_intf->set_index(index);
            }

            {
                auto conn = config->add_ingress_connections();

                auto from_intf = conn->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_PORT);
                from_intf->set_index(index);

                auto to_proc = conn->mutable_to_proc();
                to_proc->set_ptype(SwitchProcessorType::SW_PROC_APP);
                to_proc->set_index(index);
            }

            {
                auto conn = config->add_egress_connections();

                auto on_proc = conn->mutable_on_proc();
                on_proc->set_ptype(SwitchProcessorType::SW_PROC_APP);
                on_proc->set_index(index);

                auto from_intf = conn->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_PORT);
                from_intf->set_index(index);

                auto to_intf = conn->mutable_to_intf();
                to_intf->set_itype(SwitchInterfaceType::SW_INTF_HOST);
                to_intf->set_index(index);
            }
        }

        for (unsigned int index = 0; index < dev.nhosts; ++index) {
            {
                auto src = config->add_ingress_sources();

                auto from_intf = src->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_HOST);
                from_intf->set_index(index);

                auto to_intf = src->mutable_to_intf();
                to_intf->set_itype(SwitchInterfaceType::SW_INTF_HOST);
                to_intf->set_index(index);
            }

            {
                auto conn = config->add_ingress_connections();

                auto from_intf = conn->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_HOST);
                from_intf->set_index(index);

                auto to_proc = conn->mutable_to_proc();
                to_proc->set_ptype(SwitchProcessorType::SW_PROC_APP);
                to_proc->set_index(index);
            }

            {
                auto conn = config->add_egress_connections();

                auto on_proc = conn->mutable_on_proc();
                on_proc->set_ptype(SwitchProcessorType::SW_PROC_APP);
                on_proc->set_index(index);

                auto from_intf = conn->mutable_from_intf();
                from_intf->set_itype(SwitchInterfaceType::SW_INTF_HOST);
                from_intf->set_index(index);

                auto to_intf = conn->mutable_to_intf();
                to_intf->set_itype(SwitchInterfaceType::SW_INTF_PORT);
                to_intf->set_index(index);
            }
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_switch_config(
    const SwitchConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_switch_config(req, [&rdwr](const SwitchConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_switch_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetSwitchConfig(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchConfigRequest* req,
    ServerWriter<SwitchConfigResponse>* writer) {
    get_switch_config(*req, [&writer](const SwitchConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_switch_config(
    const SwitchConfigRequest& req,
    function<void(const SwitchConfigResponse&)> write_resp) {
    // TODO: the following should be in an iterator for producing devices.
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        SwitchConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    if (!req.has_config()) {
        SwitchConfigResponse resp;
        resp.set_error_code(ErrorCode::EC_MISSING_SWITCH_CONFIG);
        write_resp(resp);
        return;
    }
    auto config = req.config();

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        SwitchConfigResponse resp;

        cerr <<
            "----------------------------------------" << endl <<
            "--- " << __func__ << " Device ID: " << dev_id << endl;

        cerr << "--- " << __func__ << " ingress_sources:" << endl;
        for (auto src : config.ingress_sources()) {
            auto from_intf = src.from_intf();
            auto to_intf = src.to_intf();
            cerr <<
                "------> from_intf = " << from_intf.index() << " [" <<
                SwitchInterfaceType_Name(from_intf.itype()) << "], " <<
                "to_intf = " << to_intf.index() << " [" <<
                SwitchInterfaceType_Name(to_intf.itype()) << "]" << endl;
        }

        cerr << "--- " << __func__ << " ingress_connects:" << endl;
        for (auto conn : config.ingress_connections()) {
            auto from_intf = conn.from_intf();
            auto to_proc = conn.to_proc();
            cerr <<
                "------> from_intf = " << from_intf.index() << " [" <<
                SwitchInterfaceType_Name(from_intf.itype()) << "], " <<
                "to_proc = " << to_proc.index() << " [" <<
                SwitchProcessorType_Name(to_proc.ptype()) << "]" << endl;
        }

        cerr << "--- " << __func__ << " egress_connections:" << endl;
        for (auto conn : config.egress_connections()) {
            auto on_proc = conn.on_proc();
            auto from_intf = conn.from_intf();
            auto to_intf = conn.to_intf();
            cerr <<
                "------> on_proc = " << on_proc.index() << " [" <<
                SwitchProcessorType_Name(on_proc.ptype()) << "], " <<
                "from_intf = " << from_intf.index() << " [" <<
                SwitchInterfaceType_Name(from_intf.itype()) << "], " <<
                "to_intf = " << to_intf.index() << " [" <<
                SwitchInterfaceType_Name(to_intf.itype()) << "]" << endl;
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_switch_config(
    const SwitchConfigRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_switch_config(req, [&rdwr](const SwitchConfigResponse& resp) -> void {
        BatchResponse bresp;
        auto config = bresp.mutable_switch_config();
        config->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetSwitchConfig(
    [[maybe_unused]] ServerContext* ctx,
    const SwitchConfigRequest* req,
    ServerWriter<SwitchConfigResponse>* writer) {
    set_switch_config(*req, [&writer](const SwitchConfigResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
