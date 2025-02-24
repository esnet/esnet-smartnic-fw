#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "sn_cfg_client.hpp"
#include "sn_cfg_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SnCfgClient : public SmartnicConfigClient {
public:
    //----------------------------------------------------------------------------------------------
    void show_device_info() {
        DeviceInfoRequest req;
        req.set_dev_id(-1);

        ClientContext ctx;
        auto stream = _stub->GetDeviceInfo(&ctx, req);

        DeviceInfoResponse resp;
        while (stream->Read(&resp)) {
            auto err = resp.error_code();
            if (err != ErrorCode::EC_OK) {
                cerr << "ERROR[device]: " << ErrorCode_Name(err) << "[" << err << "]." << endl;
                continue;
            }

            auto info = resp.info();
            auto pci = info.pci();
            ostringstream vid;
            vid << "0x" << setfill('0') << setw(4) << hex << pci.vendor_id();
            ostringstream did;
            did << "0x" << setfill('0') << setw(4) << hex << pci.device_id();

            cout << "PCI Info:" << endl
                 << "    Bus ID:    " << pci.bus_id() << endl
                 << "    Vendor ID: " << vid.str() << endl
                 << "    Device ID: " << did.str() << endl;

            auto build = info.build();
            ostringstream number;
            number << "0x" << setfill('0') << setw(8) << hex << build.number();
            ostringstream status;
            status << "0x" << setfill('0') << setw(8) << hex << build.status();

            ostringstream dna;
            dna << "0x" << setfill('0') << hex;
            for (auto d : build.dna()) {
                dna << setw(8) << d;
            }

            cout << "Build:" << endl
                 << "    Number: " << number.str() << endl
                 << "    Status: " << status.str() << endl
                 << "    DNA:    " << dna.str() << endl;
        }

        auto status = stream->Finish();
        if (!status.ok()) {
            cerr << "ERROR[device]: status='" << status.error_message() << "'." << endl;
        }
    }

    //----------------------------------------------------------------------------------------------
    void disable_ports() {
        PortConfigRequest req;
        req.set_dev_id(-1);
        req.set_port_id(-1);

        auto cfg = req.mutable_config();
        cfg->set_state(PortState::PORT_STATE_DISABLE);

        ClientContext ctx;
        auto stream = _stub->SetPortConfig(&ctx, req);

        PortConfigResponse resp;
        while (stream->Read(&resp)) {
            auto err = resp.error_code();
            if (err != ErrorCode::EC_OK) {
                cerr << "ERROR[disable_ports]: " << ErrorCode_Name(err) << "[" << err << "]." << endl;
                continue;
            }
        }

        auto status = stream->Finish();
        if (!status.ok()) {
            cerr << "ERROR[disable_ports]: status='" << status.error_message() << "'." << endl;
        }
    }

    //----------------------------------------------------------------------------------------------
    void configure_defaults(int dev_id) {
        disable_ports();
        usleep(1 * 1000 * 1000); // Wait 1s for the pipelines to drain.

        ClientContext ctx;
        auto stream = _stub->Batch(&ctx);

        {
            BatchRequest breq;
            breq.set_op(BatchOperation::BOP_SET);

            auto req = breq.mutable_switch_config();
            req->set_dev_id(dev_id);

            auto cfg = req->mutable_config();
            cfg->set_bypass_mode(SwitchBypassMode::SW_BYPASS_STRAIGHT);

            for (unsigned int index = 0; index < 2; ++index) {
                {
                    auto sel = cfg->add_ingress_selectors();
                    sel->set_index(index);
                    sel->set_intf(SwitchInterface::SW_INTF_PHYSICAL);
                    sel->set_dest(SwitchDestination::SW_DEST_APP);
                }

                {
                    auto sel = cfg->add_egress_selectors();
                    sel->set_index(index);
                    sel->set_intf(SwitchInterface::SW_INTF_PHYSICAL);
                }
            }

            stream->Write(breq);
        }

        for (int host_id = 0; host_id < 2; ++host_id) {
#define N_VFS 3
#define N_QUEUES 1
            unsigned int host_base = host_id * (1 + N_VFS);

            BatchRequest breq;
            breq.set_op(BatchOperation::BOP_SET);

            auto req = breq.mutable_host_config();
            req->set_dev_id(dev_id);
            req->set_host_id(host_id);

            auto cfg = req->mutable_config();
            auto dma = cfg->mutable_dma();
            dma->set_reset(true);

            auto pf = dma->add_functions();
            auto pid = pf->mutable_func_id();
            pid->set_ftype(HostFunctionType::HOST_FUNC_PHYSICAL);
            pf->set_base_queue(host_base);
            pf->set_num_queues(N_QUEUES);

            for (unsigned int func_id = 0; func_id < N_VFS; ++func_id) {
                auto vf = dma->add_functions();
                auto vid = vf->mutable_func_id();
                vid->set_ftype(HostFunctionType::HOST_FUNC_VIRTUAL);
                vid->set_index(func_id);
                vf->set_base_queue(host_base + (1 + func_id) * N_QUEUES);
                vf->set_num_queues(N_QUEUES);
            }

            stream->Write(breq);

#undef N_VFS
#undef N_QUEUES
        }

        {
            BatchRequest breq;
            breq.set_op(BatchOperation::BOP_SET);

            auto req = breq.mutable_port_config();
            req->set_dev_id(dev_id);
            req->set_port_id(-1);

            auto cfg = req->mutable_config();
            cfg->set_state(PortState::PORT_STATE_ENABLE);
            cfg->set_fec(PortFec::PORT_FEC_REED_SOLOMON);
            cfg->set_loopback(PortLoopback::PORT_LOOPBACK_NONE);

            stream->Write(breq);
        }

        stream->WritesDone();

        BatchResponse resp;
        while (stream->Read(&resp)) {
            auto err = resp.error_code();
            if (err != ErrorCode::EC_OK) {
                cerr << "ERROR[defaults]: " << ErrorCode_Name(err) << "[" << err << "]." << endl;
                continue;
            }
        }

        auto status = stream->Finish();
        if (!status.ok()) {
            cerr << "ERROR[defaults]: status='" << status.error_message() << "'." << endl;
        }
    }
};

//--------------------------------------------------------------------------------------------------
int main() {
    auto client = new SnCfgClient;
    cout << "DEBUG[main]: is_connected="
         << (client->is_connected() ? "true" : "false")
         << "." << endl;

    client->show_device_info();
    client->configure_defaults(0);
}
