#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "GrpcChannelManager.h"
#include "NdtCommon.h"
#include <iostream>

namespace ndtapp::gate {
StatusGrpcClient::StatusGrpcClient() {
    timeoutMs_ = ConfigMgr::Instance().GetInt("Timeout", "RpcTimeoutMs", 3000);
    channel_ = GrpcChannelManager::Instance().Get(address("NDT_STATUS_HOST", "NDT_STATUS_PORT", 50052));
    stub_ = ndt::StatusService::NewStub(channel_);
}
grpc::Status StatusGrpcClient::Route(uint64_t userId, ndt::RouteReply &reply) const {
    ndt::RouteRequest request; request.set_user_id(userId);
    grpc::ClientContext context; deadline(context, timeoutMs_);
    const auto status = stub_->Route(&context, request, &reply);
    if (!status.ok()) std::cerr << "StatusServer RPC: " << status.error_message() << '\n';
    return status;
}
}
