#include "Fft.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>

namespace ndtapp {
FftResult calculateFft(const std::vector<float> &samples, double sampleRateHz) {
    if (samples.size() < 2 || samples.size() > 65536 || !std::isfinite(sampleRateHz) || sampleRateHz <= 0)
        throw std::invalid_argument("Invalid FFT input");
    std::size_t n = 1;
    while (n < samples.size()) n <<= 1;
    std::vector<std::complex<double>> values(n);
    for (std::size_t i = 0; i < samples.size(); ++i) values[i] = samples[i];
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    constexpr double pi = 3.14159265358979323846;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const auto omega = std::polar(1.0, -2.0 * pi / double(len));
        for (std::size_t start = 0; start < n; start += len) {
            std::complex<double> w(1, 0);
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto u = values[start + j];
                const auto v = values[start + j + len / 2] * w;
                values[start + j] = u + v;
                values[start + j + len / 2] = u - v;
                w *= omega;
            }
        }
    }
    FftResult result;
    result.frequencyHz.reserve(n / 2 + 1);
    result.amplitude.reserve(n / 2 + 1);
    std::size_t peak = 1;
    for (std::size_t i = 0; i <= n / 2; ++i) {
        result.frequencyHz.push_back(double(i) * sampleRateHz / double(n));
        // One-sided peak amplitude, normalized by the original sample count.
        const double edge = (i == 0 || i == n / 2) ? 1.0 : 2.0;
        const double magnitude = edge * std::abs(values[i]) / double(samples.size());
        result.amplitude.push_back(magnitude);
        if (i > 0 && magnitude > result.amplitude[peak]) peak = i;
    }
    result.dominantHz = result.frequencyHz[peak];
    return result;
}
}
