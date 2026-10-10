#include "AuthGrpcClient.h"
#include "ConfigMgr.h"
#include "GrpcChannelManager.h"
#include "NdtCommon.h"
#include <iostream>

namespace ndtapp::gate {
AuthGrpcClient::AuthGrpcClient() {
    auto &cfg = ConfigMgr::Instance();
    timeoutMs_ = cfg.GetInt("Timeout", "RpcTimeoutMs", 3000);
    channel_ = GrpcChannelManager::Instance().Get(address("NDT_AUTH_HOST", "NDT_AUTH_PORT", 50051));
    stub_ = ndt::AuthService::NewStub(channel_);
}
grpc::Status AuthGrpcClient::Verify(const std::string &username, const std::string &password,
                                     ndt::LoginReply &reply) const {
    ndt::LoginRequest request;
    request.set_username(username); request.set_password(password);
    grpc::ClientContext context; deadline(context, timeoutMs_);
    const auto status = stub_->Verify(&context, request, &reply);
    if (!status.ok()) std::cerr << "AuthServer RPC: " << status.error_message() << '\n';
    return status;
}
}
