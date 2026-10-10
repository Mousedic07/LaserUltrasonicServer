#pragma once
#include "ndt.grpc.pb.h"
#include <memory>

namespace ndtapp::gate {
class StatusGrpcClient final {
public:
    StatusGrpcClient();
    grpc::Status Route(uint64_t userId, ndt::RouteReply &reply) const;
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ndt::StatusService::Stub> stub_;
    int timeoutMs_;
};
}
