#pragma once
#include "server/logic/FileServer/LogicSystem.h"
#include "ndt.grpc.pb.h"

namespace ndtapp::file {
class FileGrpcServiceImpl final : public ndt::ArchiveService::Service {
public:
    explicit FileGrpcServiceImpl(LogicSystem &logic) : logic_(logic) {}
    grpc::Status Query(grpc::ServerContext *, const ndt::UploadInfo *, ndt::UploadProgress *) override;
    grpc::Status Append(grpc::ServerContext *, const ndt::UploadChunk *, ndt::UploadProgress *) override;
    grpc::Status Complete(grpc::ServerContext *, const ndt::UploadInfo *, ndt::UploadProgress *) override;
private:
    LogicSystem &logic_;
};
}
