#pragma once
#include "ndt.grpc.pb.h"
#include <memory>

namespace ndtapp::acq {
class StatusGrpcClient final {
public:
    StatusGrpcClient();
    grpc::Status Heartbeat(const ndt::NodeHeartbeat &heartbeat, ndt::Ack &reply) const;
    grpc::Status Validate(const std::string &token, ndt::SessionReply &reply) const;
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ndt::StatusService::Stub> stub_;
    int timeoutMs_;
};
}
