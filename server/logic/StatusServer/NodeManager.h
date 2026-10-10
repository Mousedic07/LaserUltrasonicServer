#pragma once
#include "ndt.pb.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace ndtapp::status {
struct Node {
    std::string id, host;
    uint32_t port = 0, connections = 0, tasks = 0;
    bool device = false, available = false;
    int64_t seen = 0;
};
class NodeManager final {
public:
    NodeManager();
    bool Report(const ndt::NodeHeartbeat &heartbeat);
    std::optional<Node> Select();
private:
    std::mutex mutex_;
    std::map<std::string, Node> nodes_;
    uint64_t next_ = 0;
    int64_t timeoutMs_;
};
}
