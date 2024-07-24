#include "agent.hpp"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <grpc/grpc.h>
#include "sn_cfg_v1.grpc.pb.h"

using namespace grpc;
using namespace sn_cfg::v1;
using namespace std;

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::init_server(void) {
    auto rv = timespec_get(&timestamp.start_wall, TIME_UTC);
    if (rv != TIME_UTC) {
        cerr << "ERROR: timespec_get failed with " << rv << "." << endl;
        exit(EXIT_FAILURE);
    }

    rv = clock_gettime(CLOCK_MONOTONIC, &timestamp.start_mono);
    if (rv != 0) {
        cerr << "ERROR: monotonic clock_gettime failed with " << rv << "." << endl;
        exit(EXIT_FAILURE);
    }

    cout << "--- UTC start time: "
         << put_time(gmtime(&timestamp.start_wall.tv_sec), "%Y-%m-%d %H:%M:%S %z")
         << " ["
         << timestamp.start_wall.tv_sec << "s."
         << timestamp.start_wall.tv_nsec << "ns"
         << "]"
         << endl;
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::deinit_server(void) {
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::get_server_status(
    [[maybe_unused]] const ServerStatusRequest& req,
    function<void(const ServerStatusResponse&)> write_resp) {
    ServerStatusResponse resp;
    auto err = ErrorCode::EC_OK;
    struct timespec now_mono;
    auto rv = clock_gettime(CLOCK_MONOTONIC, &now_mono);
    if (rv != 0) {
        err = ErrorCode::EC_SERVER_FAILED_GET_TIME;
    } else {
        auto status = resp.mutable_status();

        auto start_time = status->mutable_start_time();
        start_time->set_seconds(timestamp.start_wall.tv_sec);
        start_time->set_nanos(timestamp.start_wall.tv_nsec);

        struct timespec diff;
        diff.tv_sec = now_mono.tv_sec - timestamp.start_mono.tv_sec;
        diff.tv_nsec = now_mono.tv_nsec - timestamp.start_mono.tv_nsec;
        if (diff.tv_nsec < 0) {
            diff.tv_sec -= 1;
            diff.tv_nsec += 1000000000;
        }

        auto up_time = status->mutable_up_time();
        up_time->set_seconds(diff.tv_sec);
        up_time->set_nanos(diff.tv_nsec);
    }

    resp.set_error_code(err);
    write_resp(resp);
}

//--------------------------------------------------------------------------------------------------
void SmartnicConfigImpl::batch_get_server_status(
    const ServerStatusRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_server_status(req, [&rdwr](const ServerStatusResponse& resp) -> void {
        BatchResponse bresp;
        auto status = bresp.mutable_server_status();
        status->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicConfigImpl::GetServerStatus(
    [[maybe_unused]] ServerContext* ctx,
    const ServerStatusRequest* req,
    ServerWriter<ServerStatusResponse>* writer) {
    get_server_status(*req, [&writer](const ServerStatusResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
