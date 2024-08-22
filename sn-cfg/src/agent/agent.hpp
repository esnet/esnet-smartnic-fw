#ifndef AGENT_HPP
#define AGENT_HPP

#include "device.hpp"
#include "prometheus.hpp"
#include "sn_cfg_v1.grpc.pb.h"

#include <bitset>
#include <string>
#include <time.h>
#include <vector>

using namespace grpc;
using namespace sn_cfg::v1;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicConfigImpl final : public SmartnicConfig::Service {
public:
    explicit SmartnicConfigImpl(const vector<string>& bus_ids, unsigned int prometheus_port);
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

    // Module configuration.
    Status GetModuleGpio(
        ServerContext*, const ModuleGpioRequest*, ServerWriter<ModuleGpioResponse>*) override;
    Status SetModuleGpio(
        ServerContext*, const ModuleGpioRequest*, ServerWriter<ModuleGpioResponse>*) override;
    Status GetModuleInfo(
        ServerContext*, const ModuleInfoRequest*, ServerWriter<ModuleInfoResponse>*) override;
    Status GetModuleMem(
        ServerContext*, const ModuleMemRequest*, ServerWriter<ModuleMemResponse>*) override;
    Status SetModuleMem(
        ServerContext*, const ModuleMemRequest*, ServerWriter<ModuleMemResponse>*) override;
    Status GetModuleStatus(
        ServerContext*, const ModuleStatusRequest*, ServerWriter<ModuleStatusResponse>*) override;

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

    // Server configuration.
    Status GetServerConfig(
        ServerContext*, const ServerConfigRequest*, ServerWriter<ServerConfigResponse>*) override;
    Status SetServerConfig(
        ServerContext*, const ServerConfigRequest*, ServerWriter<ServerConfigResponse>*) override;
    Status GetServerStatus(
        ServerContext*, const ServerStatusRequest*, ServerWriter<ServerStatusResponse>*) override;

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

    struct {
        struct timespec start_wall;
        struct timespec start_mono;
    } timestamp;

    struct {
        bitset<ServerDebugFlag_MAX + 1> flags;
    } debug;

    const char* debug_flag_label(const ServerDebugFlag flag);

#define SERVER_LOG_IF_DEBUG(_flag, _label, _stream_statements) \
    if (this->debug.flags.test(_flag)) { \
        cerr << "DEBUG_" #_label "[" << this->debug_flag_label(_flag) << "]: " \
             << _stream_statements << endl; \
    }

    struct {
        bitset<ServerControlStatsFlag_MAX + 1> stats_flags;
    } control;

    void set_defaults(const DefaultsRequest&, function<void(const DefaultsResponse&)>);
    void batch_set_defaults(
        const DefaultsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_device(Device* dev);
    void deinit_device(Device* dev);
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

    void init_module(Device* dev);
    void deinit_module(Device* dev);
    void get_module_gpio(const ModuleGpioRequest&, function<void(const ModuleGpioResponse&)>);
    void batch_get_module_gpio(
        const ModuleGpioRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_module_gpio(const ModuleGpioRequest&, function<void(const ModuleGpioResponse&)>);
    void batch_set_module_gpio(
        const ModuleGpioRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_module_info(const ModuleInfoRequest&, function<void(const ModuleInfoResponse&)>);
    void batch_get_module_info(
        const ModuleInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_module_mem(const ModuleMemRequest&, function<void(const ModuleMemResponse&)>);
    void batch_get_module_mem(
        const ModuleMemRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_module_mem(const ModuleMemRequest&, function<void(const ModuleMemResponse&)>);
    void batch_set_module_mem(
        const ModuleMemRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_module_status(const ModuleStatusRequest&, function<void(const ModuleStatusResponse&)>);
    void batch_get_module_status(
        const ModuleStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

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

    void init_server(void);
    void deinit_server(void);
    void get_server_config(const ServerConfigRequest&, function<void(const ServerConfigResponse&)>);
    void batch_get_server_config(
        const ServerConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    ErrorCode apply_server_control_stats_flag(ServerControlStatsFlag flag, bool enable);
    void set_server_config(const ServerConfigRequest&, function<void(const ServerConfigResponse&)>);
    void batch_set_server_config(
        const ServerConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_server_status(const ServerStatusRequest&, function<void(const ServerStatusResponse&)>);
    void batch_get_server_status(
        const ServerStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

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
