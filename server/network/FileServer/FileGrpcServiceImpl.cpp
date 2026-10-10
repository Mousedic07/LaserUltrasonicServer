#include "FileGrpcServiceImpl.h"

namespace ndtapp::file {
grpc::Status FileGrpcServiceImpl::Query(grpc::ServerContext *,
    const ndt::UploadInfo *request, ndt::UploadProgress *reply) {
    try { logic_.Query(*request, *reply); return grpc::Status::OK; }
    catch (const std::exception &e) { return {grpc::StatusCode::UNAVAILABLE, e.what()}; }
}
grpc::Status FileGrpcServiceImpl::Append(grpc::ServerContext *,
    const ndt::UploadChunk *request, ndt::UploadProgress *reply) {
    try { logic_.Append(*request, *reply); return grpc::Status::OK; }
    catch (const std::invalid_argument &e) { return {grpc::StatusCode::INVALID_ARGUMENT, e.what()}; }
    catch (const std::exception &e) { return {grpc::StatusCode::UNAVAILABLE, e.what()}; }
}
grpc::Status FileGrpcServiceImpl::Complete(grpc::ServerContext *,
    const ndt::UploadInfo *request, ndt::UploadProgress *reply) {
    try { logic_.Complete(*request, *reply); return grpc::Status::OK; }
    catch (const std::exception &e) { return {grpc::StatusCode::FAILED_PRECONDITION, e.what()}; }
}
}
