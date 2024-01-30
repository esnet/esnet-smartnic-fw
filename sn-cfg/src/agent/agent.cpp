#include "agent.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <CLI/CLI.hpp>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <json/json.h>

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
        }
    }
}
*/

#define HELP_CONFIG_TLS_CERT_CHAIN "{\"server\":{\"tls\":{\"cert-chain\":\"<PATH-TO-PEM-FILE>\"}}}"
#define HELP_CONFIG_TLS_KEY        "{\"server\":{\"tls\":{\"key\":\"<PATH-TO-PEM-FILE>\"}}}"
#define HELP_CONFIG_AUTH_TOKENS    "{\"server\":{\"auth\":{\"tokens\":[\"<TOKEN1>\", ...]}}}"

#define ENV_VAR_ADDRESS        "SN_CFG_SERVER_ADDRESS"
#define ENV_VAR_PORT           "SN_CFG_SERVER_PORT"
#define ENV_VAR_TLS_CERT_CHAIN "SN_CFG_SERVER_TLS_CERT_CHAIN"
#define ENV_VAR_TLS_KEY        "SN_CFG_SERVER_TLS_KEY"
#define ENV_VAR_AUTH_TOKENS    "SN_CFG_SERVER_AUTH_TOKENS"

//--------------------------------------------------------------------------------------------------
struct Arguments {
    struct Server {
        vector<string> bus_ids;

        string address;
        unsigned int port;

        string tls_cert_chain;
        string tls_key;

        vector<string> auth_tokens;

        bool no_config_file;
        string config_file;
    } server;
};

struct Command {
    CLI::App* cmd;
    int (*run)(const Arguments&);
};

//--------------------------------------------------------------------------------------------------
SmartnicConfigImpl::SmartnicConfigImpl(const vector<string>& bus_ids) {
    cout << endl << "--- PCI bus IDs:" << endl;
    for (auto bus_id : bus_ids) {
        cout << "------> " << bus_id << endl;
        devices.push_back({
            .bus_id = bus_id,
            .nhosts = 2,
            .nports = 2,
            .base = NULL,
        });
    }
}

//--------------------------------------------------------------------------------------------------
SmartnicConfigImpl::~SmartnicConfigImpl() {
}

//--------------------------------------------------------------------------------------------------
static string read_bin_file(const string& path) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        cerr << "ERROR: Failed to open file '" << path << "'." << endl;
        exit(EXIT_FAILURE);
    }

    ostringstream out;
    out << in.rdbuf();
    if (in.fail()) {
        cerr << "ERROR: Failed to read file '" << path << "'." << endl;
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
#define METADATA_TOKEN_KEY "authorization"

        auto md = auth_metadata.find(METADATA_TOKEN_KEY);
        if (md == auth_metadata.end()) {
            return Status(StatusCode::UNAUTHENTICATED, "Missing token key from metadata.");
        }
        const auto md_token = md->second.data();

        for (const auto& token : tokens) {
            if (md_token == token) {
                return Status::OK;
            }
        }

        return Status(StatusCode::UNAUTHENTICATED, "Unknown token.");

#undef METADATA_TOKEN_KEY
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
        cerr << "ERROR: Failed to open config file '" << path << "'." << endl;
        exit(EXIT_FAILURE);
    }

    Json::CharReaderBuilder builder;
    string err;
    if (!Json::parseFromStream(builder, in, &config, &err)) {
        cerr << "ERROR: Failed to parse config file '" << path << "': " << err << endl;
        exit(EXIT_FAILURE);
    }

    if (in.fail()) {
        cerr << "ERROR: Failed to read config file '" << path << "'." << endl;
        exit(EXIT_FAILURE);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static int agent_server_run(const Arguments& args) {
    // Load the configuration file.
    Json::Value config;
    if (!args.server.no_config_file &&
        load_config_file(args.server.config_file, config)) {
        config = config["server"];
        if (!config.isObject()) {
            cerr << "ERROR: Invalid config file format." << endl;
            exit(EXIT_FAILURE);
        }
    }

    auto config_auth = config["auth"];
    auto config_tls = config["tls"];

    // Setup the RPC authentication token(s).
    vector<string> auth_tokens = args.server.auth_tokens;
    if (auth_tokens.empty()) { // Get default from config file.
        auto config_auth_tokens = config_auth["tokens"];
        if (config_auth_tokens == Json::Value::null ||
            !config_auth_tokens.isArray() ||
            config_auth_tokens.size() < 1) {
            cerr <<
                "ERROR: Missing tokens needed for authenticating clients. Specify the tokens to "
                "use with one or more --auth-token options or add to the config file as "
                HELP_CONFIG_AUTH_TOKENS "." << endl;
            exit(EXIT_FAILURE);
        }

        for (auto token : config_auth_tokens) {
            auth_tokens.push_back(token.asString());
        }
    }

    // Setup the server's certificate chain to send to clients during TLS handshake.
    auto tls_cert_chain = args.server.tls_cert_chain;
    if (tls_cert_chain.empty()) { // Get default from config file.
        auto config_tls_cert_chain = config_tls["cert-chain"];
        if (config_tls_cert_chain == Json::Value::null) {
            cerr <<
                "ERROR: Missing server certificate chain needed for sending to clients during TLS "
                "handshake. Specify the PEM file to use with the --tls-cert-chain option or add to "
                "the config file as " HELP_CONFIG_TLS_CERT_CHAIN "." << endl;
            exit(EXIT_FAILURE);
        }
        tls_cert_chain = config_tls_cert_chain.asString();
    }

    // Setup the server's private key for TLS encryption.
    auto tls_key = args.server.tls_key;
    if (tls_key.empty()) { // Get default from config file.
        auto config_tls_key = config_tls["key"];
        if (config_tls_key == Json::Value::null) {
            cerr <<
                "ERROR: Missing server private key needed for TLS encryption. Specify the PEM "
                "file to use with the --tls-key option or add to the config file as "
                HELP_CONFIG_TLS_KEY "." << endl;
            exit(EXIT_FAILURE);
        }
        tls_key = config_tls_key.asString();
    }

    // Setup the credentials for the TLS handshake.
    SslServerCredentialsOptions opts(GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
    opts.pem_key_cert_pairs.push_back({
        .private_key = read_bin_file(tls_key),
        .cert_chain = read_bin_file(tls_cert_chain),
    });
    auto credentials = SslServerCredentials(opts);

    // Setup the authentication method.
    auto processor = new AuthMetadataToken(auth_tokens);
    credentials->SetAuthMetadataProcessor(shared_ptr<typeof(*processor)>(processor));

    // Set the server's bind address and port.
    string address(args.server.address + ":" +  to_string(args.server.port));
    ServerBuilder builder;
    builder.AddListeningPort(address, credentials);

    // Attach the gRPC configuration service.
    SmartnicConfigImpl service(args.server.bus_ids);
    builder.RegisterService(&service);

    // Create the server and bind it's address.
    // TODO:
    // - Look into enabling health check
    //   - https://grpc.github.io/grpc/cpp/md_doc_health-checking.html
    //   - Python example: https://github.com/grpc/grpc/tree/master/examples/python/health_checking
    // - Look into async API
    auto server = builder.BuildAndStart();

    // Process RPCs.
    cout << __func__ << ": Serving on " << address << endl;
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
        "each token. Can also be set as a space-separated list via the " ENV_VAR_AUTH_TOKENS
        " environment variable. Default taken from config file as " HELP_CONFIG_AUTH_TOKENS ".")->
        envname(ENV_VAR_AUTH_TOKENS);

    cmd->add_flag(
        "--no-config-file", args.no_config_file,
        "Disable support for loading server configuration from a file.");
    cmd->add_option(
        "--config-file", args.config_file,
        "Server JSON configuration file.")->
        default_val(args.config_file);

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
    CLI::App app("SmartNIC Configuration Agent");
    vector<Command> commands;
    Arguments args{
        .server = {
            .bus_ids = {},

            .address = "[::]",
            .port = 50100,

            .tls_cert_chain = "",
            .tls_key = "",

            .auth_tokens = {},

            .no_config_file = false,
            .config_file = "sn-cfg.json",
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
