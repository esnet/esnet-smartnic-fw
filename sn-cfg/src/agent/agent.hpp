#ifndef AGENT_HPP
#define AGENT_HPP

#include "device.hpp"
#include "prometheus.hpp"
#include "sn_cfg_v1.grpc.pb.h"

#include <string>
#include <vector>

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicConfigImpl final : public SmartnicConfig::Service {
public:
    explicit SmartnicConfigImpl(const vector<string>& bus_ids, const string& debug_dir,
                                unsigned int prometheus_port);
    ~SmartnicConfigImpl();

    // Batching of multiple RPCs.
    Status Batch(ServerContext*, ServerReaderWriter<BatchResponse, BatchRequest>*) override;

    // Defaults configuration.
    Status SetDefaults(
        ServerContext*, const DefaultsRequest*, ServerWriter<DefaultsResponse>*) override;

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
    Status GetHostStats(
        ServerContext*, const HostStatsRequest*, ServerWriter<HostStatsResponse>*) override;
    Status ClearHostStats(
        ServerContext*, const HostStatsRequest*, ServerWriter<HostStatsResponse>*) override;

    // Port configuration.
    Status GetPortConfig(
        ServerContext*, const PortConfigRequest*, ServerWriter<PortConfigResponse>*) override;
    Status SetPortConfig(
        ServerContext*, const PortConfigRequest*, ServerWriter<PortConfigResponse>*) override;
    Status GetPortStatus(
        ServerContext*, const PortStatusRequest*, ServerWriter<PortStatusResponse>*) override;
    Status GetPortStats(
        ServerContext*, const PortStatsRequest*, ServerWriter<PortStatsResponse>*) override;
    Status ClearPortStats(
        ServerContext*, const PortStatsRequest*, ServerWriter<PortStatsResponse>*) override;

    // Statistics configuration.
    Status GetStats(ServerContext*, const StatsRequest*, ServerWriter<StatsResponse>*) override;
    Status ClearStats(ServerContext*, const StatsRequest*, ServerWriter<StatsResponse>*) override;

    // Switch configuration.
    Status GetSwitchConfig(
        ServerContext*, const SwitchConfigRequest*, ServerWriter<SwitchConfigResponse>*) override;
    Status SetSwitchConfig(
        ServerContext*, const SwitchConfigRequest*, ServerWriter<SwitchConfigResponse>*) override;
    Status GetSwitchStats(
        ServerContext*, const SwitchStatsRequest*, ServerWriter<SwitchStatsResponse>*) override;
    Status ClearSwitchStats(
        ServerContext*, const SwitchStatsRequest*, ServerWriter<SwitchStatsResponse>*) override;

private:
    vector<Device*> devices;

    struct {
        prom_collector_registry_t* registry;
        struct MHD_Daemon* daemon;
    } prometheus;

    void set_defaults(const DefaultsRequest&, function<void(const DefaultsResponse&)>);
    void batch_set_defaults(
        const DefaultsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_device_info(const DeviceInfoRequest&, function<void(const DeviceInfoResponse&)>);
    void batch_get_device_info(
        const DeviceInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_device_status(const DeviceStatusRequest&, function<void(const DeviceStatusResponse&)>);
    void batch_get_device_status(
        const DeviceStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_host(Device* dev);
    void deinit_host(Device* dev);
    void get_host_config(const HostConfigRequest&, function<void(const HostConfigResponse&)>);
    void batch_get_host_config(
        const HostConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_host_config(const HostConfigRequest&, function<void(const HostConfigResponse&)>);
    void batch_set_host_config(
        const HostConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_or_clear_host_stats(
        const HostStatsRequest&, bool, function<void(const HostStatsResponse&)>);
    void batch_get_host_stats(
        const HostStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_host_stats(
        const HostStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_port(Device* dev);
    void deinit_port(Device* dev);
    void get_port_config(const PortConfigRequest&, function<void(const PortConfigResponse&)>);
    void batch_get_port_config(
        const PortConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_port_config(const PortConfigRequest&, function<void(const PortConfigResponse&)>);
    void batch_set_port_config(
        const PortConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_port_status(const PortStatusRequest&, function<void(const PortStatusResponse&)>);
    void batch_get_port_status(
        const PortStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_or_clear_port_stats(
        const PortStatsRequest&, bool, function<void(const PortStatsResponse&)>);
    void batch_get_port_stats(
        const PortStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_port_stats(
        const PortStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_or_clear_stats(const StatsRequest&, bool, function<void(const StatsResponse&)>);
    void batch_get_stats(const StatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_stats(const StatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_switch(Device* dev);
    void deinit_switch(Device* dev);
    void get_switch_config(const SwitchConfigRequest&, function<void(const SwitchConfigResponse&)>);
    void batch_get_switch_config(
        const SwitchConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_switch_config(const SwitchConfigRequest&, function<void(const SwitchConfigResponse&)>);
    void batch_set_switch_config(
        const SwitchConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_or_clear_switch_stats(
        const SwitchStatsRequest&, bool, function<void(const SwitchStatsResponse&)>);
    void batch_get_switch_stats(
        const SwitchStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_switch_stats(
        const SwitchStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
};

#endif // AGENT_HPP
