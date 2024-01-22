#ifndef AGENT_HPP
#define AGENT_HPP

#include "sn_cfg_v1.grpc.pb.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicConfigImpl final : public SmartnicConfig::Service {
public:
    explicit SmartnicConfigImpl();
    ~SmartnicConfigImpl();
};

#endif // AGENT_HPP
