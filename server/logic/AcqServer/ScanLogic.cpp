#include "ScanLogic.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ndtapp::acq {
double ScanLogic::Number(const boost::json::object &obj, const char *key) {
    const auto &value = obj.at(key);
    if (value.is_double()) return value.as_double();
    if (value.is_int64()) return double(value.as_int64());
    if (value.is_uint64()) return double(value.as_uint64());
    throw std::invalid_argument(std::string("non-numeric ") + key);
}
uint64_t ScanLogic::Positive(const boost::json::object &obj, const char *key) {
    const double value = Number(obj, key);
    if (!std::isfinite(value) || value < 1 || value != std::floor(value))
        throw std::invalid_argument(key);
    return uint64_t(value);
}
uint64_t ScanLogic::Nonnegative(const boost::json::object &obj, const char *key) {
    const double value = Number(obj, key);
    if (!std::isfinite(value) || value < 0 || value != std::floor(value))
        throw std::invalid_argument(key);
    return uint64_t(value);
}
ScanConfig ScanLogic::Parse(const boost::json::object &obj) {
    ScanConfig c;
    c.rate = Number(obj, "sample_rate_hz");
    c.excitation = Number(obj, "excitation_frequency_hz");
    c.step = Number(obj, "scan_step_mm");
    c.samples = uint32_t(Positive(obj, "sample_count"));
    c.points = uint32_t(Positive(obj, "scan_points"));
    c.channels = uint32_t(Positive(obj, "channel_count"));
    if (!std::isfinite(c.rate) || !std::isfinite(c.excitation) || !std::isfinite(c.step) ||
        c.rate <= c.excitation * 2 || c.excitation <= 0 || c.step <= 0 ||
        c.samples < 64 || c.samples > 16384 || c.points > 10000 || c.channels > 16)
        throw std::invalid_argument("invalid scan config or Nyquist violation");
    c.totalBytes = uint64_t(c.points) * c.channels * (52 + uint64_t(c.samples) * 4);
    if (c.totalBytes > uint64_t(4) * 1024 * 1024 * 1024)
        throw std::invalid_argument("scan file exceeds 4 GiB limit");
    return c;
}
namespace {
void appendLittle(std::string &out, uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(char(value >> (i * 8)));
}
void appendDouble(std::string &out, double value) {
    uint64_t bits; std::memcpy(&bits, &value, 8); appendLittle(out, bits, 8);
}
}
std::string ScanLogic::EncodeRecord(uint64_t frameId, const boost::json::object &frame,
                                    uint32_t sampleCount) {
    std::string out("WAV1", 4);
    appendLittle(out, frameId, 8);
    appendLittle(out, Nonnegative(frame, "channel_id"), 4);
    appendLittle(out, Positive(frame, "timestamp_ms"), 8);
    appendDouble(out, Number(frame, "position_x_mm"));
    appendDouble(out, Number(frame, "position_y_mm"));
    appendDouble(out, Number(frame, "sample_rate_hz"));
    appendLittle(out, sampleCount, 4);
    const auto &samples = frame.at("samples").as_array();
    if (samples.size() != sampleCount) throw std::runtime_error("DAQ sample count changed");
    for (const auto &sample : samples) {
        float value = float(sample.is_double() ? sample.as_double() : double(sample.as_int64()));
        uint32_t bits; std::memcpy(&bits, &value, 4); appendLittle(out, bits, 4);
    }
    return out;
}
}
