#include "FileGrpcClient.h"
#include "ConfigMgr.h"
#include "GrpcChannelManager.h"
#include "NdtCommon.h"
#include <iostream>

namespace ndtapp::acq {
FileGrpcClient::FileGrpcClient() {
    auto &cfg = ConfigMgr::Instance();
    queryMs_ = cfg.GetInt("Timeout", "RpcTimeoutMs", 3000);
    appendMs_ = cfg.GetInt("Timeout", "FileAppendTimeoutMs", 5000);
    completeMs_ = cfg.GetInt("Timeout", "FileCompleteTimeoutMs", 30000);
    channel_ = GrpcChannelManager::Instance().Get(address("NDT_FILE_HOST", "NDT_FILE_PORT", 50054));
    stub_ = ndt::ArchiveService::NewStub(channel_);
}
grpc::Status FileGrpcClient::Query(const ndt::UploadInfo &info, ndt::UploadProgress &progress) const {
    grpc::ClientContext context; deadline(context, queryMs_);
    const auto status = stub_->Query(&context, info, &progress);
    if (!status.ok()) std::cerr << "FileServer Query: " << status.error_message() << '\n';
    return status;
}
grpc::Status FileGrpcClient::Append(const ndt::UploadChunk &chunk, ndt::UploadProgress &progress) const {
    grpc::ClientContext context; deadline(context, appendMs_);
    const auto status = stub_->Append(&context, chunk, &progress);
    if (!status.ok()) std::cerr << "FileServer Append: " << status.error_message() << '\n';
    return status;
}
grpc::Status FileGrpcClient::Complete(const ndt::UploadInfo &info, ndt::UploadProgress &progress) const {
    grpc::ClientContext context; deadline(context, completeMs_);
    const auto status = stub_->Complete(&context, info, &progress);
    if (!status.ok()) std::cerr << "FileServer Complete: " << status.error_message() << '\n';
    return status;
}
}
