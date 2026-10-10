#include "FftLogic.h"

namespace ndtapp::proc {
FftResult FftLogic::Calculate(const std::vector<float> &samples, double sampleRateHz) const {
    return calculateFft(samples, sampleRateHz);
}
}
