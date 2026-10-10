#pragma once
#include "server/logic/ProcServer/LogicSystem.h"
#include "ndt.grpc.pb.h"

namespace ndtapp::proc {
class ProcGrpcServiceImpl final : public ndt::ProcessService::Service {
public:
    explicit ProcGrpcServiceImpl(LogicSystem &logic) : logic_(logic) {}
    grpc::Status Fft(grpc::ServerContext *, const ndt::Waveform *, ndt::Spectrum *) override;
private:
    LogicSystem &logic_;
};
}
