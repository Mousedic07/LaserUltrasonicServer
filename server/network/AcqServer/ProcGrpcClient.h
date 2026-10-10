#pragma once
#include "ndt.grpc.pb.h"
#include <memory>

namespace ndtapp::acq {
class ProcGrpcClient final {
public:
    ProcGrpcClient();
    grpc::Status Fft(const ndt::Waveform &wave, ndt::Spectrum &spectrum) const;
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ndt::ProcessService::Stub> stub_;
    int timeoutMs_;
};
}
