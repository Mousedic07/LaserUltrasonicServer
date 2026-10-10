#pragma once
#include "Fft.h"
#include <vector>

namespace ndtapp::proc {
class FftLogic final {
public:
    FftResult Calculate(const std::vector<float> &samples, double sampleRateHz) const;
};
}
