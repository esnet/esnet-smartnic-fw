#ifndef AGENT_HPP
#define AGENT_HPP

#include "device.hpp"
#include "prometheus.hpp"
#include "sn_p4_v2.grpc.pb.h"

#include <bitset>
#include <ctime>
#include <string>
#include <vector>

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicP4Impl final : public SmartnicP4::Service {
public:
    explicit SmartnicP4Impl(const vector<string>& bus_ids, unsigned int prometheus_port);
    ~SmartnicP4Impl();

    // Batching of multiple RPCs.
    Status Batch(ServerContext*, ServerReaderWriter<BatchResponse, BatchRequest>*) override;

    // Device configuration.
    Status GetDeviceInfo(
        ServerContext*, const DeviceInfoRequest*, ServerWriter<DeviceInfoResponse>*) override;

    // Pipeline configuration.
    Status GetPipelineInfo(
        ServerContext*, const PipelineInfoRequest*, ServerWriter<PipelineInfoResponse>*) override;
    Status GetPipelineStats(
        ServerContext*, const PipelineStatsRequest*, ServerWriter<PipelineStatsResponse>*) override;
    Status ClearPipelineStats(
        ServerContext*, const PipelineStatsRequest*, ServerWriter<PipelineStatsResponse>*) override;

    // Table configuration.
    Status ClearTable(ServerContext*, const TableRequest*, ServerWriter<TableResponse>*) override;
    Status InsertTableRule(
        ServerContext*, const TableRuleRequest*, ServerWriter<TableRuleResponse>*) override;
    Status DeleteTableRule(
        ServerContext*, const TableRuleRequest*, ServerWriter<TableRuleResponse>*) override;

    // Server configuration.
    Status GetServerConfig(
        ServerContext*, const ServerConfigRequest*, ServerWriter<ServerConfigResponse>*) override;
    Status SetServerConfig(
        ServerContext*, const ServerConfigRequest*, ServerWriter<ServerConfigResponse>*) override;
    Status GetServerStatus(
        ServerContext*, const ServerStatusRequest*, ServerWriter<ServerStatusResponse>*) override;

    // Stats configuration.
    Status GetStats(ServerContext*, const StatsRequest*, ServerWriter<StatsResponse>*) override;
    Status ClearStats(ServerContext*, const StatsRequest*, ServerWriter<StatsResponse>*) override;

    bool get_server_times(struct timespec* start, struct timespec* up);

private:
    vector<Device*> devices;

    struct {
        prom_collector_registry_t* registry;
        struct MHD_Daemon* daemon;
    } prometheus;

    struct ServerStats {
        struct stats_zone* zone;
    };
    struct {
        struct stats_domain* domain;
        ServerStats* status;
    } server_stats;

    struct {
        struct timespec start_wall;
        struct timespec start_mono;
    } timestamp;

    struct {
        bitset<ServerDebugFlag_MAX + 1> flags;
    } debug;

    const char* debug_flag_label(const ServerDebugFlag flag);

#define _SERVER_LOG(_label_str, _log_type, _stream_statements) \
    cerr << #_log_type "[" << _label_str << "]: " << _stream_statements << flush
#define SERVER_LOG(_label, _log_type, _stream_statements) \
    _SERVER_LOG(#_label, _log_type, _stream_statements)
#define SERVER_LOG_LINE(_label, _log_type, _stream_statements) \
    SERVER_LOG(_label, _log_type, _stream_statements << endl)

#define SERVER_LOG_INIT(_label, _log_type, _stream_statements) \
    SERVER_LOG(_label, INIT_##_log_type, _stream_statements)
#define SERVER_LOG_LINE_INIT(_label, _log_type, _stream_statements) \
    SERVER_LOG_INIT(_label, _log_type, _stream_statements << endl)

#define SERVER_LOG_DEBUG(_label_str, _log_type, _stream_statements) \
    _SERVER_LOG(_label_str, DEBUG_##_log_type, _stream_statements)
#define SERVER_LOG_LINE_DEBUG(_flag, _log_type, _stream_statements) \
    _SERVER_LOG(this->debug_flag_label(_flag), DEBUG_##_log_type, _stream_statements << endl)
#define SERVER_LOG_IF_DEBUG(_flag, _log_type, _stream_statements) \
    if (this->debug.flags.test(_flag)) { \
        SERVER_LOG_LINE_DEBUG(_flag, _log_type, _stream_statements); \
    }

    void get_device_info(const DeviceInfoRequest&, function<void(const DeviceInfoResponse&)>);
    void batch_get_device_info(
        const DeviceInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    const struct snp4_info_table* pipeline_get_table_info(
        const DevicePipeline* pipeline, const string& table_name);
    bool pipeline_has_table(const DevicePipeline* pipeline, const string& table_name);
    const struct snp4_info_action* pipeline_get_table_action_info(
        const struct snp4_info_table* ti, const string& action_name);
    bool pipeline_has_table_action(const struct snp4_info_table* ti, const string& action_name);
    const struct snp4_info_counter_block* pipeline_get_counter_block_info(
        const DevicePipeline* pipeline, const string& block_name);
    bool pipeline_has_counter_block(const DevicePipeline* pipeline, const string& block_name);

    void init_pipeline(Device* dev);
    void deinit_pipeline(Device* dev);
    void get_pipeline_info(const PipelineInfoRequest&, function<void(const PipelineInfoResponse&)>);
    void batch_get_pipeline_info(
        const PipelineInfoRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_or_clear_pipeline_stats(
        const PipelineStatsRequest&, bool do_clear, function<void(const PipelineStatsResponse&)>);
    void batch_get_pipeline_stats(
        const PipelineStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_pipeline_stats(
        const PipelineStatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_counters(Device* dev, DevicePipeline* pipeline);
    void deinit_counters(DevicePipeline* pipeline);

    void init_table_ecc(Device* dev, DevicePipeline* pipeline);
    void deinit_table_ecc(DevicePipeline* pipeline);

    void clear_table(const TableRequest&, function<void(const TableResponse&)>);
    void batch_clear_table(const TableRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void insert_or_delete_table_rule(
        const TableRuleRequest&, bool do_insert, function<void(const TableRuleResponse&)>);
    void batch_insert_table_rule(
        const TableRuleRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_delete_table_rule(
        const TableRuleRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void init_server(void);
    void init_server_stats(void);
    void deinit_server(void);
    void get_server_config(const ServerConfigRequest&, function<void(const ServerConfigResponse&)>);
    void batch_get_server_config(
        const ServerConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void set_server_config(const ServerConfigRequest&, function<void(const ServerConfigResponse&)>);
    void batch_set_server_config(
        const ServerConfigRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void get_server_status(const ServerStatusRequest&, function<void(const ServerStatusResponse&)>);
    void batch_get_server_status(
        const ServerStatusRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);

    void get_or_clear_stats(
        const StatsRequest&, bool do_clear, function<void(const StatsResponse&)>);
    void batch_get_stats(
        const StatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
    void batch_clear_stats(
        const StatsRequest&, ServerReaderWriter<BatchResponse, BatchRequest>*);
};

#endif // AGENT_HPP
