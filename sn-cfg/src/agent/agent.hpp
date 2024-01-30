#ifndef AGENT_HPP
#define AGENT_HPP

#include "device.hpp"
#include "sn_cfg_v1.grpc.pb.h"

#include <string>
#include <vector>

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicConfigImpl final : public SmartnicConfig::Service {
public:
    explicit SmartnicConfigImpl(const vector<string>& bus_ids);
    ~SmartnicConfigImpl();

    // Batching of multiple RPCs.
    Status Batch(ServerContext*, ServerReaderWriter<BatchResponse, BatchRequest>*) override;

    // General device configuration.
    Status GetDeviceInfo(
        ServerContext*, const DeviceInfoRequest*, ServerWriter<DeviceInfoResponse>*) override;
    Status GetDeviceStatus(
        ServerContext*, const DeviceStatusRequest*, ServerWriter<DeviceStatusResponse>*) override;

    // Host configuration.
    Status GetHostConfig(
        ServerContext*, const HostConfigRequest*, ServerWriter<HostConfigResponse>*) override;
    Status SetHostConfig(
        ServerContext*, const HostConfigRequest*, ServerWriter<HostConfigResponse>*) override;

    // Port configuration.
    Status GetPortConfig(
        ServerContext*, const PortConfigRequest*, ServerWriter<PortConfigResponse>*) override;
    Status SetPortConfig(
        ServerContext*, const PortConfigRequest*, ServerWriter<PortConfigResponse>*) override;
    Status GetPortStatus(
        ServerContext*, const PortStatusRequest*, ServerWriter<PortStatusResponse>*) override;

    // Switch configuration.
    Status GetSwitchConfig(
        ServerContext*, const SwitchConfigRequest*, ServerWriter<SwitchConfigResponse>*) override;
    Status SetSwitchConfig(
        ServerContext*, const SwitchConfigRequest*, ServerWriter<SwitchConfigResponse>*) override;

private:
    vector<Device> devices;

    void get_device_info(const DeviceInfoRequest&, function<void(const DeviceInfoResponse&)>);
    void batch_get_device_info(
        const DeviceInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_device_status(const DeviceStatusRequest&, function<void(const DeviceStatusResponse&)>);
    void batch_get_device_status(
        const DeviceStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_host_config(const HostConfigRequest&, function<void(const HostConfigResponse&)>);
    void batch_get_host_config(
        const HostConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_host_config(const HostConfigRequest&, function<void(const HostConfigResponse&)>);
    void batch_set_host_config(
        const HostConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_port_config(const PortConfigRequest&, function<void(const PortConfigResponse&)>);
    void batch_get_port_config(
        const PortConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_port_config(const PortConfigRequest&, function<void(const PortConfigResponse&)>);
    void batch_set_port_config(
        const PortConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_port_status(const PortStatusRequest&, function<void(const PortStatusResponse&)>);
    void batch_get_port_status(
        const PortStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_switch_config(const SwitchConfigRequest&, function<void(const SwitchConfigResponse&)>);
    void batch_get_switch_config(
        const SwitchConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_switch_config(const SwitchConfigRequest&, function<void(const SwitchConfigResponse&)>);
    void batch_set_switch_config(
        const SwitchConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
};

#endif // AGENT_HPP
