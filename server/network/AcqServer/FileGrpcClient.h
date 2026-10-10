#pragma once
#include "ndt.grpc.pb.h"
#include <memory>

namespace ndtapp::acq {
class FileGrpcClient final {
public:
    FileGrpcClient();
    grpc::Status Query(const ndt::UploadInfo &info, ndt::UploadProgress &progress) const;
    grpc::Status Append(const ndt::UploadChunk &chunk, ndt::UploadProgress &progress) const;
    grpc::Status Complete(const ndt::UploadInfo &info, ndt::UploadProgress &progress) const;
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ndt::ArchiveService::Stub> stub_;
    int queryMs_, appendMs_, completeMs_;
};
}
