#include "NodeManager.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include <vector>

namespace ndtapp::status {
NodeManager::NodeManager() : timeoutMs_(ConfigMgr::Instance().GetInt(
    "Timeout", "HeartbeatMs", 6000, "NDT_HEARTBEAT_TIMEOUT_MS")) {}
bool NodeManager::Report(const ndt::NodeHeartbeat &request) {
    if (request.node_id().empty() || request.host().empty() ||
        request.port() == 0 || request.port() > 65535) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[request.node_id()] = Node{request.node_id(), request.host(), request.port(),
        request.active_connections(), request.running_tasks(), request.device_connected(),
        request.available(), epochMs()};
    return true;
}
std::optional<Node> NodeManager::Select() {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t now = epochMs();
    uint64_t best = UINT64_MAX;
    std::vector<Node> candidates;
    for (const auto &entry : nodes_) {
        const Node &node = entry.second;
        if (!node.available || !node.device || now - node.seen > timeoutMs_) continue;
        const uint64_t load = uint64_t(node.connections) + uint64_t(node.tasks) * 2;
        if (load < best) { best = load; candidates.clear(); }
        if (load == best) candidates.push_back(node);
    }
    if (candidates.empty()) return std::nullopt;
    return candidates[next_++ % candidates.size()];
}
}
