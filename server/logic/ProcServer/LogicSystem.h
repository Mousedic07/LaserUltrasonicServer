#pragma once
#include "FftLogic.h"
#include "ndt.pb.h"

namespace ndtapp::proc {
class LogicSystem final {
public:
    void Process(const ndt::Waveform &request, ndt::Spectrum &reply) const;
private:
    FftLogic fft_;
};
}
