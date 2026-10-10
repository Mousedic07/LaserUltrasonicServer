#include "LogicSystem.h"

namespace ndtapp::proc {
void LogicSystem::Process(const ndt::Waveform &request, ndt::Spectrum &reply) const {
    const std::vector<float> samples(request.samples().begin(), request.samples().end());
    const auto result = fft_.Calculate(samples, request.sample_rate_hz());
    reply.set_scan_id(request.scan_id()); reply.set_frame_id(request.frame_id());
    reply.set_channel_id(request.channel_id());
    for (double x : result.frequencyHz) reply.add_frequency_bins_hz(x);
    for (double y : result.amplitude) reply.add_amplitude_spectrum(y);
    reply.set_dominant_frequency_hz(result.dominantHz);
}
}
