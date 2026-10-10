#include "ProcGrpcServiceImpl.h"

namespace ndtapp::proc {
grpc::Status ProcGrpcServiceImpl::Fft(grpc::ServerContext *,
    const ndt::Waveform *request, ndt::Spectrum *reply) {
    try { logic_.Process(*request, *reply); return grpc::Status::OK; }
    catch (const std::exception &e) { return {grpc::StatusCode::INVALID_ARGUMENT, e.what()}; }
}
}
