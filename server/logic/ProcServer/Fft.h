#pragma once
#include <vector>

namespace ndtapp {
struct FftResult {
    std::vector<double> frequencyHz;
    std::vector<double> amplitude;
    double dominantHz = 0;
};
FftResult calculateFft(const std::vector<float> &samples, double sampleRateHz);
}
