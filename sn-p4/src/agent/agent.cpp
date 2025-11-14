#include "agent.hpp"
#include "prometheus.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <CLI/CLI.hpp>

#include <grpc/grpc.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <json/json.h>

#include "smartnic.h"
#include "stats.h"

using namespace google::protobuf;
using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
/*
  JSON config file format:

{
    "client": {
        "tls": {
            "ca-chain": "server-ca-chain.pem"
        },
        "auth": {
            "token": "abc"
        }
    },
    "server": {
        "tls": {
            "cert-chain": "server-chain.pem",
            "key": "server.key",
        },
        "auth": {
            "tokens": [
                "abc",
                "def"
            ]
        },
        "debug": {
            "flags": [
                "first-flag",
                "second-flag"
            ]
        }
    }
}
*/

#define HELP_CONFIG_TLS_CERT_CHAIN "{\"server\":{\"tls\":{\"cert-chain\":\"<PATH-TO-PEM-FILE>\"}}}"
#define HELP_CONFIG_TLS_KEY        "{\"server\":{\"tls\":{\"key\":\"<PATH-TO-PEM-FILE>\"}}}"
#define HELP_CONFIG_AUTH_TOKENS    "{\"server\":{\"auth\":{\"tokens\":[\"<TOKEN1>\", ...]}}}"
#define HELP_CONFIG_DEBUG_FLAGS    "{\"server\":{\"debug\":{\"flags\":[\"<FLAG1>\", ...]}}}"

#define ENV_VAR_ADDRESS         "SN_P4_SERVER_ADDRESS"
#define ENV_VAR_PORT            "SN_P4_SERVER_PORT"
#define ENV_VAR_PROMETHEUS_PORT "SN_P4_SERVER_PROMETHEUS_PORT"
#define ENV_VAR_TLS_CERT_CHAIN  "SN_P4_SERVER_TLS_CERT_CHAIN"
#define ENV_VAR_TLS_KEY         "SN_P4_SERVER_TLS_KEY"
#define ENV_VAR_AUTH_TOKENS     "SN_P4_SERVER_AUTH_TOKENS"
#define ENV_VAR_DEBUG_FLAGS     "SN_P4_SERVER_DEBUG_FLAGS"

//--------------------------------------------------------------------------------------------------
struct Arguments {
    struct Server {
        vector<string> bus_ids;

        string address;
        unsigned int port;

        unsigned int prometheus_port;

        string tls_cert_chain;
        string tls_key;

        vector<string> auth_tokens;

        bool no_config_file;
        string config_file;

        vector<string> debug_flags;
    } server;
};

struct Command {
    CLI::App* cmd;
    int (*run)(const Arguments&);
};

//--------------------------------------------------------------------------------------------------
SmartnicP4Impl::SmartnicP4Impl(const vector<string>& bus_ids,
                               const vector<string>& debug_flags,
                               unsigned int prometheus_port) {
    int rv = prom_collector_registry_default_init();
    if (rv != 0) {
        SERVER_LOG_LINE_INIT(ctor, ERROR, "Failed to init default prometheus registry.");
        exit(EXIT_FAILURE);
    }
    prometheus.registry = PROM_COLLECTOR_REGISTRY_DEFAULT;

    // Process debug flags early to allow them to be used during device initialization.
    init_server_debug(debug_flags);

    for (auto bus_id : bus_ids) {
        SERVER_LOG_LINE_INIT(ctor, INFO, "Mapping PCIe BAR2 of device " << bus_id);
        volatile struct esnet_smartnic_bar2* bar2 = smartnic_map_bar2_by_pciaddr(bus_id.c_str());
        if (bar2 == NULL) {
            SERVER_LOG_LINE_INIT(ctor, ERROR, "Failed to map PCIe BAR2 of device " << bus_id);
            exit(EXIT_FAILURE);
        }

        auto dev = new Device{
            .bus_id = bus_id,
            .bar2 = bar2,
            .pipelines = {},
            .stats = {},
        };

        struct stats_domain_spec spec = {
            .name = dev->bus_id.c_str(),
            .thread = {
                .name = NULL,
                .interval_ms = 0,
            },
            .prometheus = {
                .registry = prometheus.registry,
            },
        };

        for (auto dom = 0; dom < DeviceStatsDomain::NDOMAINS; ++dom) {
            const char* dname = device_stats_domain_name((DeviceStatsDomain)dom);
            unsigned int seconds;
            switch (dom) {
            case DeviceStatsDomain::COUNTERS:
                seconds = 1;
                break;

            default:
                seconds = 10;
                break;
            }
            spec.thread.interval_ms = seconds * 1000;
            spec.thread.name = dname;

            SERVER_LOG_LINE_INIT(ctor, INFO,
                "Allocating statistics domain '" << dname << "' on device " << bus_id);
            dev->stats.domains[dom] = stats_domain_alloc(&spec);
            if (dev->stats.domains[dom] == NULL) {
                SERVER_LOG_LINE_INIT(ctor, ERROR,
                    "Failed to allocate statistics domain '" << dname << "' on device " << bus_id);
                exit(EXIT_FAILURE);
            }
        }

        init_pipeline(dev);

        SERVER_LOG_LINE_INIT(ctor, INFO, "Starting statistics collection on device " << bus_id);
        for (auto domain : dev->stats.domains) {
            stats_domain_clear_metrics(domain, NULL);
            stats_domain_start(domain);
        }

        devices.push_back(dev);
        SERVER_LOG_LINE_INIT(ctor, INFO, "Completed init of device " << bus_id);
    }

    SERVER_LOG_LINE_INIT(ctor, INFO, "Allocating statistics domain for server");
    struct stats_domain_spec server_spec = {
        .name = "sn-p4",
        .thread = {
            .name = "server_stats",
            .interval_ms = 1000,
        },
        .prometheus = {
            .registry = prometheus.registry,
        },
    };
    server_stats.domain = stats_domain_alloc(&server_spec);
    if (server_stats.domain == NULL) {
        SERVER_LOG_LINE_INIT(ctor, ERROR, "Failed to allocate statistics domain for server");
        exit(EXIT_FAILURE);
    }

    init_server();

    SERVER_LOG_LINE_INIT(ctor, INFO, "Starting statistics collection for server");
    stats_domain_clear_metrics(server_stats.domain, NULL);
    stats_domain_start(server_stats.domain);

    SERVER_LOG_LINE_INIT(ctor, INFO, "Starting Prometheus daemon on port " << prometheus_port);
    promhttp_set_active_collector_registry(prometheus.registry);
    prometheus.daemon = promhttp_start_daemon(
        MHD_USE_SELECT_INTERNALLY, prometheus_port, NULL, NULL);
    if (prometheus.daemon == NULL) {
        SERVER_LOG_LINE_INIT(ctor, ERROR, "Failed to start prometheus daemon");
        exit(EXIT_FAILURE);
    }
}

//--------------------------------------------------------------------------------------------------
SmartnicP4Impl::~SmartnicP4Impl() {
    MHD_stop_daemon(prometheus.daemon);

    deinit_server();
    stats_domain_free(server_stats.domain);

    while (!devices.empty()) {
        auto dev = devices.back();

        deinit_pipeline(dev);

        for (auto domain : dev->stats.domains) {
            stats_domain_free(domain);
        }

        smartnic_unmap_bar2(dev->bar2);

        devices.pop_back();
        delete dev;
    }

    prom_collector_registry_destroy(prometheus.registry);
}

//--------------------------------------------------------------------------------------------------
static string read_bin_file(const string& path) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        SERVER_LOG_LINE_INIT(agent, ERROR, "Failed to open file '" << path << "'");
        exit(EXIT_FAILURE);
    }

    ostringstream out;
    out << in.rdbuf();
    if (in.fail()) {
        SERVER_LOG_LINE_INIT(agent, ERROR, "Failed to read file '" << path << "'");
        exit(EXIT_FAILURE);
    }

    return out.str();
}

//--------------------------------------------------------------------------------------------------
class AuthMetadataToken : public AuthMetadataProcessor  {
public:
    explicit AuthMetadataToken(const vector<string>& auth_tokens) {
        for (const auto& token : auth_tokens) {
            tokens.push_back("Bearer " + token);
        }
    }

    bool IsBlocking() const override { return false; }

    Status Process(const InputMetadata& auth_metadata,
                   [[maybe_unused]] AuthContext* context,
                   [[maybe_unused]] OutputMetadata* consumed_auth_metadata,
                   [[maybe_unused]] OutputMetadata* response_metadata) override {
        auto path = auth_metadata.find(":path");
        if (path != auth_metadata.end() && path->second.starts_with("/grpc.health.v1.Health/")) {
            // Allow health checking without the token.
            return Status::OK;
        }

        auto md = auth_metadata.find("authorization");
        if (md == auth_metadata.end()) {
            return Status(StatusCode::UNAUTHENTICATED, "Missing token key from metadata.");
        }
        const auto md_token = md->second;

        for (const auto& token : tokens) {
            if (md_token == token) {
                return Status::OK;
            }
        }

        return Status(StatusCode::UNAUTHENTICATED, "Unknown token.");
    }

private:
    vector<string> tokens;
};

//--------------------------------------------------------------------------------------------------
static bool load_config_file(const string& path, Json::Value& config) {
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }

    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        SERVER_LOG_LINE_INIT(agent, ERROR, "Failed to open config file '" << path << "'");
        exit(EXIT_FAILURE);
    }

    Json::CharReaderBuilder builder;
    string err;
    if (!Json::parseFromStream(builder, in, &config, &err)) {
        SERVER_LOG_LINE_INIT(agent, ERROR, "Failed to parse config file '" << path << "': " << err);
        exit(EXIT_FAILURE);
    }

    if (in.fail()) {
        SERVER_LOG_LINE_INIT(agent, ERROR, "Failed to read config file '" << path << "'");
        exit(EXIT_FAILURE);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static void parse_env_list(const vector<string>& in_list, vector<string>& out_list) {
    regex delim_re(":");
    for (auto item : in_list) {
        sregex_token_iterator parts(item.begin(), item.end(), delim_re, -1);
        sregex_token_iterator parts_end;
        for (; parts != parts_end; ++parts) {
            out_list.push_back(*parts);
        }
    }
}

//--------------------------------------------------------------------------------------------------
static int agent_server_run(const Arguments& args) {
    // Load the configuration file.
    Json::Value config;
    if (!args.server.no_config_file &&
        load_config_file(args.server.config_file, config)) {
        config = config["server"];
        if (!config.isObject()) {
            SERVER_LOG_LINE_INIT(agent, ERROR, "Invalid config file format");
            exit(EXIT_FAILURE);
        }
    }

    auto config_auth = config["auth"];
    auto config_debug = config["debug"];
    auto config_tls = config["tls"];

    // Setup the debug flags to be enabled during init.
    vector<string> debug_flags;
    if (args.server.debug_flags.empty()) { // Get default from config file.
        auto config_debug_flags = config_debug["flags"];
        if (config_debug_flags != Json::Value::null) {
            if (!config_debug_flags.isArray()) {
                SERVER_LOG_LINE_INIT(agent, ERROR,
                    "Invalid debug flags list. Specify the debug flags to use with one or more "
                    "--debug-flag options, in the environment as " ENV_VAR_DEBUG_FLAGS " or add "
                    "to the config file as " HELP_CONFIG_DEBUG_FLAGS);
                exit(EXIT_FAILURE);
            }

            for (auto flag : config_debug_flags) {
                debug_flags.push_back(flag.asString());
            }
        }
    } else {
        parse_env_list(args.server.debug_flags, debug_flags);
    }

    // Setup the RPC authentication token(s).
    vector<string> auth_tokens;
    if (args.server.auth_tokens.empty()) { // Get default from config file.
        auto config_auth_tokens = config_auth["tokens"];
        if (config_auth_tokens == Json::Value::null ||
            !config_auth_tokens.isArray() ||
            config_auth_tokens.size() < 1) {
            SERVER_LOG_LINE_INIT(agent, ERROR,
                "Missing tokens needed for authenticating clients. Specify the tokens to "
                "use with one or more --auth-token options, in the envionment as "
                ENV_VAR_AUTH_TOKENS " or add to the config file as " HELP_CONFIG_AUTH_TOKENS);
            exit(EXIT_FAILURE);
        }

        for (auto token : config_auth_tokens) {
            auth_tokens.push_back(token.asString());
        }
    } else {
        parse_env_list(args.server.auth_tokens, auth_tokens);
    }

    // Setup the server's certificate chain to send to clients during TLS handshake.
    auto tls_cert_chain = args.server.tls_cert_chain;
    if (tls_cert_chain.empty()) { // Get default from config file.
        auto config_tls_cert_chain = config_tls["cert-chain"];
        if (config_tls_cert_chain == Json::Value::null) {
            SERVER_LOG_LINE_INIT(agent, ERROR,
                "Missing server certificate chain needed for sending to clients during TLS "
                "handshake. Specify the PEM file to use with the --tls-cert-chain option or "
                "add to the config file as " HELP_CONFIG_TLS_CERT_CHAIN);
            exit(EXIT_FAILURE);
        }
        tls_cert_chain = config_tls_cert_chain.asString();
    }

    // Setup the server's private key for TLS encryption.
    auto tls_key = args.server.tls_key;
    if (tls_key.empty()) { // Get default from config file.
        auto config_tls_key = config_tls["key"];
        if (config_tls_key == Json::Value::null) {
            SERVER_LOG_LINE_INIT(agent, ERROR,
                "Missing server private key needed for TLS encryption. Specify the PEM "
                "file to use with the --tls-key option or add to the config file as "
                HELP_CONFIG_TLS_KEY);
            exit(EXIT_FAILURE);
        }
        tls_key = config_tls_key.asString();
    }

    // Setup the credentials for the TLS handshake.
    SERVER_LOG_LINE_INIT(agent, INFO,
        "TLS: key_path='" << tls_key << "', cert_chain_path='" << tls_cert_chain << "'");
    SslServerCredentialsOptions opts(GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
    opts.pem_key_cert_pairs.push_back({
        .private_key = read_bin_file(tls_key),
        .cert_chain = read_bin_file(tls_cert_chain),
    });
    auto credentials = SslServerCredentials(opts);

    // Setup the authentication method.
    auto processor = new AuthMetadataToken(auth_tokens);
    credentials->SetAuthMetadataProcessor(shared_ptr<typeof(*processor)>(processor));

    // Enable the builtin health checking protocol.
    EnableDefaultHealthCheckService(true);
    reflection::InitProtoReflectionServerBuilderPlugin();

    // Set the server's bind address and port.
    string address(args.server.address + ":" +  to_string(args.server.port));
    ServerBuilder builder;
    builder.AddListeningPort(address, credentials);

    // Attach the gRPC configuration service.
    SmartnicP4Impl service(args.server.bus_ids, debug_flags, args.server.prometheus_port);
    builder.RegisterService(&service);

    // Create the server and bind it's address.
    auto server = builder.BuildAndStart();
    auto health_check = server->GetHealthCheckService();
    health_check->SetServingStatus(true);

    // Process RPCs.
    SERVER_LOG_LINE_INIT(agent, INFO, "Serving on '" << address << "'");
    server->Wait();

    // Delete all global objects allocated by libprotobuf to keep memory checkers happy.
    ShutdownProtobufLibrary();

    return 0;
}

//--------------------------------------------------------------------------------------------------
static string pci_bus_id_check([[maybe_unused]] const string& value) {
    // TODO: validate the pci bus id format
    return string(); // Non-empty error message on error. Empty on success.
}

//--------------------------------------------------------------------------------------------------
static void agent_server_add(CLI::App& app, Arguments::Server& args, vector<Command>& commands) {
    // Create the sub-command.
    auto cmd = app.add_subcommand("server", "Run the agent server.");

    // Setup the optional arguments.
    cmd->add_option(
        "--address", args.address,
        "Address the agent will bind to. Can also be set via the " ENV_VAR_ADDRESS " environment "
        "variable.")->
        envname(ENV_VAR_ADDRESS)->
        default_val(args.address);
    cmd->add_option(
        "--port", args.port,
        "Port the agent will listen on. Can also be set via the " ENV_VAR_PORT " environment "
        "variable.")->
        envname(ENV_VAR_PORT)->
        default_val(args.port);

    cmd->add_option(
        "--prometheus-port", args.prometheus_port,
        "Port the Prometheus HTTP daemon will listen on. Can also be set via the "
        ENV_VAR_PROMETHEUS_PORT " environment variable.")->
        envname(ENV_VAR_PROMETHEUS_PORT)->
        default_val(args.prometheus_port);

    cmd->add_option(
        "--tls-cert-chain", args.tls_cert_chain,
        "Server X.509 certificate chain for TLS authentication. The chain must contain the server "
        "certificate followed by all intermediate signing certificates in order (certificate i+1 "
        "was used to sign certificate i in the chain, with i=0 being the server). Can also be set "
        "via the " ENV_VAR_TLS_CERT_CHAIN " environment variable. Default taken from config file "
        "as " HELP_CONFIG_TLS_CERT_CHAIN ".")->
        envname(ENV_VAR_TLS_CERT_CHAIN);
    cmd->add_option(
        "--tls-key", args.tls_key,
        "Private key paired to the public key contained within the server X.509 certificate. Can "
        "also be set via the " ENV_VAR_TLS_KEY " environment variable. Default taken from config "
        "file as " HELP_CONFIG_TLS_KEY ".")->
        envname(ENV_VAR_TLS_KEY);

    cmd->add_option(
        "--auth-token", args.auth_tokens,
        "Token to use for authenticating remote procedure calls received from clients. Repeat for "
        "each token. Can also be set as a colon-separated (:) list via the " ENV_VAR_AUTH_TOKENS
        " environment variable. Default taken from config file as " HELP_CONFIG_AUTH_TOKENS ".")->
        envname(ENV_VAR_AUTH_TOKENS);

    cmd->add_flag(
        "--no-config-file", args.no_config_file,
        "Disable support for loading server configuration from a file.");
    cmd->add_option(
        "--config-file", args.config_file,
        "Server JSON configuration file.")->
        default_val(args.config_file);

    cmd->add_option(
        "--debug-flag", args.debug_flags,
        "Name of a debug flag to enable automatically during init. By default, debug flags are "
        "disabled. Can also be set as a colon-separated (:) list via the " ENV_VAR_DEBUG_FLAGS
        " environment variable. Default taken from config file as " HELP_CONFIG_DEBUG_FLAGS ".")->
        envname(ENV_VAR_DEBUG_FLAGS);

    // Setup the positional arguments.
    cmd->add_option(
        "bus-ids", args.bus_ids,
        "PCI bus IDs of the devices to be managed by the agent. Each ID must be given in the form "
        "<domain>:<bus>:<device>.<function>=<HHHH>:<HH>:<HH>.<H>, where H is a single hexadecimal "
        "digit and the ranges of each fields are: domain in [0000,ffff], bus in [00,ff], device in "
        "[00,1f] and function in [0,7].")->
        check(pci_bus_id_check)-> // or transform?, each?
        expected(1, -1)->
        required();

    // Register the sub-command.
    commands.push_back({.cmd = cmd, .run = agent_server_run});
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    CLI::App app("SmartNIC P4 Agent");
    vector<Command> commands;
    Arguments args{
        .server = {
            .bus_ids = {},

            .address = "[::]",
            .port = 50050,

            .prometheus_port = 8000,

            .tls_cert_chain = "",
            .tls_key = "",

            .auth_tokens = {},

            .no_config_file = false,
            .config_file = "sn-p4.json",

            .debug_flags = {},
        },
    };

    // Setup the sub-commands.
    agent_server_add(app, args.server, commands);

    // Parse the command line.
    CLI11_PARSE(app, argc, argv);

    // Process the selected sub-command.
    for (auto c : commands) {
        if (c.cmd->parsed()) {
            return c.run(args);
        }
    }

    return 0;
}
