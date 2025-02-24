#ifndef SN_P4_CLIENT_HPP
#define SN_P4_CLIENT_HPP

#include <string>

#include "sn_p4_v2.grpc.pb.h"

//--------------------------------------------------------------------------------------------------
class SmartnicP4Client {
public:
    struct Options {
        std::string address;
        uint16_t port;
        std::string auth_token;
        std::string tls_root_certs;
        std::string tls_hostname_override;
        bool tls_insecure;

        std::string get_address() const;
        uint16_t get_port() const;
        std::string get_auth_token() const;
        std::string get_tls_root_certs() const;
        std::string get_tls_hostname_override() const;
        bool get_tls_insecure() const;
    };
    typedef std::shared_ptr<sn_p4::v2::SmartnicP4::Stub> Stub;

    static Stub connect(const Options& opts);
    bool is_connected() const { return _stub != nullptr; }
    SmartnicP4Client(const Options& opts = Options()) : _stub(connect(opts)) {}

protected:
    Stub _stub;
};

#endif // SN_P4_CLIENT_HPP
