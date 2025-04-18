#include "agent.hpp"

#include <grpc/grpc.h>
#include "sn_cfg_v2.grpc.pb.h"

#include "esnet_smartnic_toplevel.h"
#include "cmac.h"
#include "qdma.h"
#include "switch.h"

#include <unistd.h>

using namespace grpc;
using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
static ErrorCode set_defaults_one_to_one(const Device& dev) {
    // Disable ports.
    for (unsigned int index = 0; index < dev.nports; ++index) {
        volatile typeof(dev.bar2->cmac0)* cmac;
        switch (index) {
        case 0: cmac = &dev.bar2->cmac0; break;
        case 1: cmac = &dev.bar2->cmac1; break;
        default:
            return ErrorCode::EC_UNSUPPORTED_PORT_ID;
        }

        cmac_loopback_disable(cmac);
        cmac_disable(cmac);
    }

    usleep(1 * 1000 * 1000); // Wait 1s for the pipelines to drain.

    // Default the switch.
    switch_set_defaults_one_to_one(&dev.bar2->smartnic_regs);

    // Default the host interfaces.
    for (unsigned int index = 0; index < dev.nhosts; ++index) {
#define NUM_QUEUES 1
        if (!qdma_function_set_queues(dev.bar2, index, qdma_function_PF,
                                      index * NUM_QUEUES, NUM_QUEUES)) {
            return ErrorCode::EC_FAILED_SET_HOST_FUNCTION_DMA_QUEUES;
        }

#ifdef SN_CFG_HOST_SET_QDMA_CHANNEL
        if (!qdma_channel_set_queues(dev.bar2, index, index * NUM_QUEUES, NUM_QUEUES)) {
            return ErrorCode::EC_FAILED_SET_HOST_FUNCTION_DMA_QUEUES;
        }
#endif
#undef NUM_QUEUES
    }

    // Enable the ports.
    for (unsigned int index = 0; index < dev.nports; ++index) {
        volatile typeof(dev.bar2->cmac0)* cmac;
        switch (index) {
        case 0: cmac = &dev.bar2->cmac0; break;
        case 1: cmac = &dev.bar2->cmac1; break;
        default:
            return ErrorCode::EC_UNSUPPORTED_PORT_ID;
        }

        cmac_rsfec_enable(cmac);
        cmac_enable(cmac);
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::set_defaults(
    const DefaultsRequest& req,
    function<void(const DefaultsResponse&)> write_resp) {
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        DefaultsResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];
        auto err = ErrorCode::EC_OK;
        DefaultsResponse resp;

        switch (req.profile()) {
        case DefaultsProfile::DS_ONE_TO_ONE:
            err = set_defaults_one_to_one(*dev);
            break;

        case DefaultsProfile::DS_UNKNOWN:
        default:
            err = ErrorCode::EC_UNKNOWN_DEFAULTS_PROFILE;
            break;
        }

        resp.set_error_code(err);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_set_defaults(
    const DefaultsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_defaults(req, [&rdwr](const DefaultsResponse& resp) -> void {
        BatchResponse bresp;
        auto defaults = bresp.mutable_defaults();
        defaults->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::SetDefaults(
    [[maybe_unused]] ServerContext* ctx,
    const DefaultsRequest* req,
    ServerWriter<DefaultsResponse>* writer) {
    set_defaults(*req, [&writer](const DefaultsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
