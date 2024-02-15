#ifndef AGENT_HPP
#define AGENT_HPP

#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace std;

//--------------------------------------------------------------------------------------------------
class SmartnicConfigImpl final : public SmartnicConfig::Service {
public:
    explicit SmartnicConfigImpl();
    ~SmartnicConfigImpl();

    // Batching of multiple RPCs.
    Status Batch(ServerContext*, ServerReaderWriter<BatchResponse, BatchRequest>*) override;
};

#endif // AGENT_HPP
