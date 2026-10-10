#include "FftTask.h"
#include "server/network/AcqServer/CSession.h"

namespace ndtapp::acq {
void FftTask::Run(std::shared_ptr<CSession> session, BoundedQueue<ndt::Waveform> &queue,
                  ProcGrpcClient &client) {
    ndt::Waveform wave;
    while (queue.pop(wave)) {
        ndt::Spectrum spectrum;
        const auto status = client.Fft(wave, spectrum);
        if (!status.ok()) {
            session->Send(8, {{"error","ProcServer: " + status.error_message()}});
            continue;
        }
        boost::json::array bins, amplitudes;
        for (double x : spectrum.frequency_bins_hz()) bins.push_back(x);
        for (double y : spectrum.amplitude_spectrum()) amplitudes.push_back(y);
        session->Send(5, {{"scan_id",spectrum.scan_id()},{"frame_id",spectrum.frame_id()},
            {"channel_id",spectrum.channel_id()},{"frequency_bins_hz",std::move(bins)},
            {"amplitude_spectrum",std::move(amplitudes)},
            {"dominant_frequency_hz",spectrum.dominant_frequency_hz()}});
    }
}
}
