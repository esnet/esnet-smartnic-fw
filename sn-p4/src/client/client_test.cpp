#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "sn_p4_client.hpp"
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SnP4Client : public SmartnicP4Client {
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
};

//--------------------------------------------------------------------------------------------------
int main() {
    auto client = new SnP4Client;
    cout << "DEBUG[main]: is_connected="
         << (client->is_connected() ? "true" : "false")
         << "." << endl;

    client->show_device_info();
}
