#pragma once
#include <boost/property_tree/ptree.hpp>
#include <mutex>
#include <string>

namespace ndtapp {
// Adapted from llfcchat ConfigMgr: explicit file selection, typed reads and
// centralized environment overrides. Never dumps configuration values.
class ConfigMgr final {
public:
    static ConfigMgr &Instance();
    void Load(const std::string &service, int argc = 0, char **argv = nullptr);
    std::string Get(const std::string &section, const std::string &key,
                    const std::string &fallback = {}, const char *envKey = nullptr) const;
    std::string Required(const std::string &section, const std::string &key,
                         const char *envKey = nullptr) const;
    int GetInt(const std::string &section, const std::string &key, int fallback,
               const char *envKey = nullptr) const;
    double GetDouble(const std::string &section, const std::string &key, double fallback,
                     const char *envKey = nullptr) const;
    bool GetBool(const std::string &section, const std::string &key, bool fallback,
                 const char *envKey = nullptr) const;
    std::string Legacy(const std::string &envKey, const std::string &fallback) const;
    std::string Path() const;
private:
    ConfigMgr() = default;
    mutable std::mutex mutex_;
    boost::property_tree::ptree tree_;
    std::string service_, path_;
};
}
