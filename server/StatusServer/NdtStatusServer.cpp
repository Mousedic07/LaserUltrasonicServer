#include "NdtCommon.h"
#include "ndt.grpc.pb.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>

namespace {
struct Node {
    std::string id, host;
    uint32_t port = 0, connections = 0, tasks = 0;
    bool device = false, available = false;
    int64_t seen = 0;
};
struct Session { uint64_t user = 0; int64_t expires = 0; };

class StatusLogic final : public ndt::StatusService::Service {
public:
    grpc::Status Heartbeat(grpc::ServerContext *, const ndt::NodeHeartbeat *request,
                           ndt::Ack *reply) override {
        if (request->node_id().empty() || request->host().empty() || request->port() == 0 || request->port() > 65535) {
            reply->set_error("invalid heartbeat"); return grpc::Status::OK;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        nodes_[request->node_id()] = Node{request->node_id(), request->host(), request->port(),
            request->active_connections(), request->running_tasks(), request->device_connected(),
            request->available(), ndtapp::epochMs()};
        reply->set_ok(true);
        return grpc::Status::OK;
    }
    grpc::Status Route(grpc::ServerContext *, const ndt::RouteRequest *request,
                        ndt::RouteReply *reply) override {
        if (!request->user_id()) { reply->set_error("invalid user"); return grpc::Status::OK; }
        std::lock_guard<std::mutex> lock(mutex_);
        const int64_t now = ndtapp::epochMs();
        const int64_t timeout = ndtapp::envInt("NDT_HEARTBEAT_TIMEOUT_MS", 6000);
        uint64_t bestLoad = UINT64_MAX;
        std::vector<Node> candidates;
        for (const auto &entry : nodes_) {
            const Node &node = entry.second;
            if (!node.available || !node.device || now - node.seen > timeout) continue;
            const uint64_t load = uint64_t(node.connections) + uint64_t(node.tasks) * 2;
            if (load < bestLoad) { bestLoad = load; candidates.clear(); }
            if (load == bestLoad) candidates.push_back(node);
        }
        if (candidates.empty()) { reply->set_error("no healthy AcqServer"); return grpc::Status::OK; }
        const Node &node = candidates[next_++ % candidates.size()];
        const std::string token = ndtapp::randomHex(24);
        sessions_[token] = Session{request->user_id(), now + 5 * 60 * 1000};
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second.expires < now) it = sessions_.erase(it); else ++it;
        }
        reply->set_ok(true); reply->set_host(node.host); reply->set_port(node.port);
        reply->set_token(token); reply->set_node_id(node.id);
        return grpc::Status::OK;
    }
    grpc::Status Validate(grpc::ServerContext *, const ndt::SessionRequest *request,
                           ndt::SessionReply *reply) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(request->token());
        if (it == sessions_.end() || it->second.expires < ndtapp::epochMs()) {
            reply->set_error("session expired or invalid"); return grpc::Status::OK;
        }
        reply->set_ok(true); reply->set_user_id(it->second.user);
        return grpc::Status::OK;
    }
private:
    std::mutex mutex_;
    std::map<std::string, Node> nodes_;
    std::unordered_map<std::string, Session> sessions_;
    uint64_t next_ = 0;
};
}

int main() {
    const std::string listen = ndtapp::address("NDT_STATUS_HOST", "NDT_STATUS_PORT", 50052);
    StatusLogic service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server) { std::cerr << "StatusServer bind failed: " << listen << '\n'; return 1; }
    std::cout << "StatusServer listening on " << listen << std::endl;
    server->Wait();
}
