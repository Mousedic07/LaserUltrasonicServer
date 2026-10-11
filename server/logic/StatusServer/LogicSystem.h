#pragma once
#include "NodeManager.h"
#include "ndt.pb.h"

namespace ndtapp::status {
class LogicSystem final {
public:
    bool Heartbeat(const ndt::NodeHeartbeat &request) { return nodes_.Report(request); }
    void Route(const ndt::RouteRequest &request, ndt::RouteReply &reply);
private:
    NodeManager nodes_;
};
}
