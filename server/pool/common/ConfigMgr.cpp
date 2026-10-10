#include "ConfigMgr.h"
#include <boost/property_tree/ini_parser.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>

namespace ndtapp {
namespace {
std::string overrideValue(const char *key) {
    if (!key) return {};
    const char *value = std::getenv(key);
    return value && *value ? std::string(value) : std::string();
}
}
ConfigMgr &ConfigMgr::Instance() { static ConfigMgr value; return value; }
void ConfigMgr::Load(const std::string &service, int argc, char **argv) {
    namespace fs = std::filesystem;
    std::string selected = overrideValue("NDT_CONFIG");
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) selected = argv[++i];
        else if (arg.rfind("--config=", 0) == 0) selected = arg.substr(9);
    }
    if (selected.empty()) {
        const fs::path local = fs::current_path() / "config" / service / "config.ini";
        const fs::path example = fs::current_path() / "config" / service / "config.example.ini";
        selected = fs::exists(local) ? local.string() : example.string();
    }
    boost::property_tree::ptree parsed;
    try { boost::property_tree::read_ini(selected, parsed); }
    catch (const std::exception &e) {
        throw std::runtime_error("ConfigMgr cannot load " + selected + ": " + e.what());
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tree_.swap(parsed); service_ = service; path_ = selected;
    }
    std::cout << "ConfigMgr " << service << " file: " << selected << std::endl;
}
std::string ConfigMgr::Get(const std::string &section, const std::string &key,
                           const std::string &fallback, const char *envKey) const {
    const std::string override = overrideValue(envKey);
    if (!override.empty()) return override;
    std::lock_guard<std::mutex> lock(mutex_);
    return tree_.get<std::string>(section + "." + key, fallback);
}
std::string ConfigMgr::Required(const std::string &section, const std::string &key,
                                const char *envKey) const {
    const std::string value = Get(section, key, {}, envKey);
    if (value.empty()) throw std::runtime_error("ConfigMgr missing " + section + "." + key);
    return value;
}
int ConfigMgr::GetInt(const std::string &section, const std::string &key, int fallback,
                      const char *envKey) const {
    const std::string value = Get(section, key, std::to_string(fallback), envKey);
    try { std::size_t n = 0; int result = std::stoi(value, &n); if (n == value.size()) return result; }
    catch (...) {}
    throw std::runtime_error("ConfigMgr invalid integer " + section + "." + key);
}
double ConfigMgr::GetDouble(const std::string &section, const std::string &key, double fallback,
                            const char *envKey) const {
    const std::string value = Get(section, key, std::to_string(fallback), envKey);
    try { std::size_t n = 0; double result = std::stod(value, &n); if (n == value.size()) return result; }
    catch (...) {}
    throw std::runtime_error("ConfigMgr invalid number " + section + "." + key);
}
bool ConfigMgr::GetBool(const std::string &section, const std::string &key, bool fallback,
                        const char *envKey) const {
    std::string value = Get(section, key, fallback ? "true" : "false", envKey);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (value == "true" || value == "1" || value == "yes") return true;
    if (value == "false" || value == "0" || value == "no") return false;
    throw std::runtime_error("ConfigMgr invalid boolean " + section + "." + key);
}
std::string ConfigMgr::Legacy(const std::string &key, const std::string &fallback) const {
    // The legacy accessor keeps existing integration scripts functional while
    // all actual environment reads now pass through this one component.
    const std::string override = overrideValue(key.c_str());
    if (!override.empty()) return override;
    static const std::map<std::string, std::pair<std::string,std::string>> keys = {
        {"NDT_GATE_HOST",{"GateServer","Host"}}, {"NDT_GATE_PORT",{"GateServer","Port"}},
        {"NDT_AUTH_HOST",{"AuthServer","Host"}}, {"NDT_AUTH_PORT",{"AuthServer","Port"}},
        {"NDT_STATUS_HOST",{"StatusServer","Host"}}, {"NDT_STATUS_PORT",{"StatusServer","Port"}},
        {"NDT_ACQ_HOST",{"AcqServer","Host"}}, {"NDT_ACQ_PORT",{"AcqServer","Port"}},
        {"NDT_ACQ_NODE_ID",{"AcqServer","NodeId"}},
        {"NDT_PROC_HOST",{"ProcServer","Host"}}, {"NDT_PROC_PORT",{"ProcServer","Port"}},
        {"NDT_FILE_HOST",{"FileServer","Host"}}, {"NDT_FILE_PORT",{"FileServer","Port"}},
        {"NDT_MYSQL_HOST",{"MySQL","Host"}}, {"NDT_MYSQL_PORT",{"MySQL","Port"}},
        {"NDT_MYSQL_USER",{"MySQL","User"}}, {"NDT_MYSQL_DATABASE",{"MySQL","Database"}},
        {"NDT_MYSQL_PASSWORD",{"MySQL","Password"}},
        {"NDT_REDIS_HOST",{"Redis","Host"}}, {"NDT_REDIS_PORT",{"Redis","Port"}},
        {"NDT_REDIS_PASSWORD",{"Redis","Password"}},
        {"NDT_FILE_ROOT",{"Storage","Root"}}, {"NDT_SPOOL_ROOT",{"Storage","SpoolRoot"}},
        {"NDT_DEVICE_HOST",{"Device","Host"}}, {"NDT_DEVICE_BASE_PORT",{"Device","BasePort"}},
        {"NDT_HEARTBEAT_TIMEOUT_MS",{"Timeout","HeartbeatMs"}},
        {"NDT_JWT_SECRET",{"Security","JwtSecret"}}
    };
    const auto found = keys.find(key);
    if (found == keys.end()) return fallback;
    std::lock_guard<std::mutex> lock(mutex_);
    // A service's own endpoint lives in [Server], not its peer section.
    const std::string section = found->second.first == service_ ? "Server" : found->second.first;
    const std::string field = found->second.second;
    return tree_.get<std::string>(section + "." + field, fallback);
}
std::string ConfigMgr::Path() const { std::lock_guard<std::mutex> lock(mutex_); return path_; }
}
