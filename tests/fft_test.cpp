#include "../server/ProcServer/Fft.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    constexpr double rate = 20e6, frequency = 2.75e6;
    constexpr std::size_t count = 4096;
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = float(std::sin(2 * 3.14159265358979323846 * frequency * i / rate));
    const auto result = ndtapp::calculateFft(samples, rate);
    assert(result.frequencyHz.size() == count / 2 + 1);
    assert(std::abs(result.dominantHz - frequency) <= rate / count);
    const std::size_t peak = std::size_t(std::round(result.dominantHz * count / rate));
    assert(result.amplitude[peak] > 0.8 && result.amplitude[peak] < 1.1);
    std::cout << "FFT peak=" << result.dominantHz << " Hz amplitude=" << result.amplitude[peak] << "\n";
}
