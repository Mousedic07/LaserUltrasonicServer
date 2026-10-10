#pragma once
#include <boost/json.hpp>
#include <cstdint>
#include <string>

namespace ndtapp::acq {
struct ScanConfig {
    double rate = 0, excitation = 0, step = 0;
    uint32_t samples = 0, points = 0, channels = 0;
    uint64_t totalBytes = 0;
};
class ScanLogic final {
public:
    static ScanConfig Parse(const boost::json::object &value);
    static double Number(const boost::json::object &value, const char *key);
    static uint64_t Positive(const boost::json::object &value, const char *key);
    static uint64_t Nonnegative(const boost::json::object &value, const char *key);
    static std::string EncodeRecord(uint64_t frameId, const boost::json::object &frame,
                                    uint32_t sampleCount);
};
}
