#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "GrpcChannelManager.h"
#include "NdtCommon.h"
#include <iostream>

namespace ndtapp::acq {
StatusGrpcClient::StatusGrpcClient() {
    timeoutMs_ = ConfigMgr::Instance().GetInt("Timeout", "RpcTimeoutMs", 3000);
    channel_ = GrpcChannelManager::Instance().Get(address("NDT_STATUS_HOST", "NDT_STATUS_PORT", 50052));
    stub_ = ndt::StatusService::NewStub(channel_);
}
grpc::Status StatusGrpcClient::Heartbeat(const ndt::NodeHeartbeat &heartbeat, ndt::Ack &reply) const {
    grpc::ClientContext context; deadline(context, timeoutMs_);
    const auto status = stub_->Heartbeat(&context, heartbeat, &reply);
    if (!status.ok()) std::cerr << "StatusServer heartbeat: " << status.error_message() << '\n';
    return status;
}
grpc::Status StatusGrpcClient::Validate(const std::string &token, ndt::SessionReply &reply) const {
    ndt::SessionRequest request; request.set_token(token);
    grpc::ClientContext context; deadline(context, timeoutMs_);
    const auto status = stub_->Validate(&context, request, &reply);
    if (!status.ok()) std::cerr << "StatusServer validate: " << status.error_message() << '\n';
    return status;
}
}
