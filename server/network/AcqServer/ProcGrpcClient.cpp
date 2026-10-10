#include "ProcGrpcClient.h"
#include "ConfigMgr.h"
#include "GrpcChannelManager.h"
#include "NdtCommon.h"
#include <iostream>

namespace ndtapp::acq {
ProcGrpcClient::ProcGrpcClient() {
    timeoutMs_ = ConfigMgr::Instance().GetInt("Timeout", "FftTimeoutMs", 5000);
    channel_ = GrpcChannelManager::Instance().Get(address("NDT_PROC_HOST", "NDT_PROC_PORT", 50053));
    stub_ = ndt::ProcessService::NewStub(channel_);
}
grpc::Status ProcGrpcClient::Fft(const ndt::Waveform &wave, ndt::Spectrum &spectrum) const {
    grpc::ClientContext context; deadline(context, timeoutMs_);
    const auto status = stub_->Fft(&context, wave, &spectrum);
    if (!status.ok()) std::cerr << "ProcServer FFT: " << status.error_message() << '\n';
    return status;
}
}
