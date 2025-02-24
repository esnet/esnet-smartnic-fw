#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "sn_cfg_client.hpp"
#include "sn_cfg_v2.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

using namespace sn_cfg::v2;
using namespace std;

#define DEFAULT_ADDRESS  "ip6-localhost"
#define DEFAULT_PORT     50100
#define SERVER_CERT_COPY "/root/smartnic-cfg-server.pem"

//--------------------------------------------------------------------------------------------------
string SmartnicConfigClient::Options::get_address() const {
    if (!address.empty()) {
        return string(address);
    }

    const char* env = getenv("SN_CFG_CLI_ADDRESS");
    if (env == NULL) {
        env = DEFAULT_ADDRESS;
    }
    return string(env);
}

//--------------------------------------------------------------------------------------------------
uint16_t SmartnicConfigClient::Options::get_port() const {
    if (port != 0) {
        return port;
    }

    const char* env = getenv("SN_CFG_CLI_PORT");
    if (env == NULL) {
        return DEFAULT_PORT;
    }
    return stoi(env, 0, 10);
}

//--------------------------------------------------------------------------------------------------
string SmartnicConfigClient::Options::get_auth_token() const {
    if (!auth_token.empty()) {
        return string(auth_token);
    }

    const char* env = getenv("SN_CFG_CLI_AUTH_TOKEN");
    if (env == NULL) {
        env = "";
    }
    return string(env);
}

//--------------------------------------------------------------------------------------------------
string SmartnicConfigClient::Options::get_tls_root_certs() const {
    if (!tls_root_certs.empty()) {
        return string(tls_root_certs);
    }

    const char* env = getenv("SN_CFG_CLI_TLS_ROOT_CERTS");
    if (env == NULL) {
        env = "";
    }
    return string(env);
}

//--------------------------------------------------------------------------------------------------
string SmartnicConfigClient::Options::get_tls_hostname_override() const {
    if (!tls_hostname_override.empty()) {
        return string(tls_hostname_override);
    }

    const char* env = getenv("SN_CFG_CLI_TLS_HOSTNAME_OVERRIDE");
    if (env == NULL) {
        env = "";
    }
    return string(env);
}

//--------------------------------------------------------------------------------------------------
bool SmartnicConfigClient::Options::get_tls_insecure() const {
    if (tls_insecure) {
        return true;
    }

    const char* env = getenv("SN_CFG_CLI_TLS_INSECURE");
    if (env == NULL) {
        return false;
    }

    string value(env);
    transform(value.begin(), value.end(), value.begin(), [](char c){ return tolower(c); });
    return value == "1" || value == "t" || value == "true";
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
class AuthMetadataToken : public grpc::MetadataCredentialsPlugin {
public:
    AuthMetadataToken(const string& token) {
        _token = "Bearer " + token;
    }

    bool IsBlocking() const override { return false; }

    grpc::Status GetMetadata([[maybe_unused]] grpc::string_ref service_url,
                             [[maybe_unused]] grpc::string_ref method_name,
                             [[maybe_unused]] const grpc::AuthContext& channel_auth_context,
                             multimap<grpc::string, grpc::string>* metadata) override {
        metadata->insert(make_pair("authorization", _token));
        return grpc::Status::OK;
    }

 private:
    string _token;
};

//--------------------------------------------------------------------------------------------------
SmartnicConfigClient::Stub SmartnicConfigClient::connect(
    const SmartnicConfigClient::Options& opts) {
    ostringstream server;
    server << opts.get_address() << ":" << opts.get_port();

    string server_cert;
    if (opts.get_tls_insecure()) {
        server_cert = SERVER_CERT_COPY;
        if (!filesystem::exists(server_cert)) {
            string cmd = "openssl s_client -connect " + server.str() +
                         " -showcerts 2>/dev/null | openssl x509 2>/dev/null >" + server_cert;
            int rv = system(cmd.c_str());
            if (rv != 0) {
                cerr << "ERROR: Failed to retrieve server TLS cert [rv=" << rv << "]." << endl;
                filesystem::remove(server_cert);
                return nullptr;
            }
        }
    } else {
        server_cert = opts.get_tls_root_certs();
    }

    grpc::ChannelArguments channel_args;
    auto tls_hostname_override = opts.get_tls_hostname_override();
    if (!tls_hostname_override.empty()) {
        channel_args.SetSslTargetNameOverride(tls_hostname_override);
    }

    auto tls_opts = grpc::SslCredentialsOptions();
    if (!server_cert.empty()) {
        tls_opts.pem_root_certs = read_bin_file(server_cert);
    }
    auto tls_creds = grpc::SslCredentials(tls_opts);

    auto plugin = new AuthMetadataToken(opts.get_auth_token());
    auto call_creds = grpc::MetadataCredentialsFromPlugin(unique_ptr<typeof(*plugin)>(plugin));

    auto channel_creds = grpc::CompositeChannelCredentials(tls_creds, call_creds);
    auto channel = grpc::CreateCustomChannel(server.str(), channel_creds, channel_args);

    return SmartnicConfig::NewStub(channel);
}
