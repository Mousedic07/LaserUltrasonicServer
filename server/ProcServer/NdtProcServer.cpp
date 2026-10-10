#include "NdtCommon.h"
#include "Fft.h"
#include "ndt.grpc.pb.h"
#include <iostream>

namespace {
class ProcessLogic final : public ndt::ProcessService::Service {
public:
    grpc::Status Fft(grpc::ServerContext *, const ndt::Waveform *request,
                      ndt::Spectrum *reply) override {
        try {
            std::vector<float> samples(request->samples().begin(), request->samples().end());
            const auto result = ndtapp::calculateFft(samples, request->sample_rate_hz());
            reply->set_scan_id(request->scan_id()); reply->set_frame_id(request->frame_id());
            reply->set_channel_id(request->channel_id());
            for (double x : result.frequencyHz) reply->add_frequency_bins_hz(x);
            for (double y : result.amplitude) reply->add_amplitude_spectrum(y);
            reply->set_dominant_frequency_hz(result.dominantHz);
            return grpc::Status::OK;
        } catch (const std::exception &e) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
        }
    }
};
}
int main() {
    const std::string listen = ndtapp::address("NDT_PROC_HOST", "NDT_PROC_PORT", 50053);
    ProcessLogic service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server) { std::cerr << "ProcServer bind failed: " << listen << '\n'; return 1; }
    std::cout << "ProcServer listening on " << listen << std::endl;
    server->Wait();
}
