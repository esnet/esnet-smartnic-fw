#ifndef AGENT_HPP
#define AGENT_HPP

#include "device.hpp"
#include "sn_p4_v2.grpc.pb.h"

#include <string>
#include <vector>

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicP4Impl final : public SmartnicP4::Service {
public:
    explicit SmartnicP4Impl(const vector<string>& bus_ids);
    ~SmartnicP4Impl();

    // Batching of multiple RPCs.
    Status Batch(ServerContext*, ServerReaderWriter<BatchResponse, BatchRequest>*) override;

    // Device configuration.
    Status GetDeviceInfo(
        ServerContext*, const DeviceInfoRequest*, ServerWriter<DeviceInfoResponse>*) override;

private:
    vector<Device*> devices;

    void get_device_info(const DeviceInfoRequest&, function<void(const DeviceInfoResponse&)>);
    void batch_get_device_info(
        const DeviceInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
};

#endif // AGENT_HPP
