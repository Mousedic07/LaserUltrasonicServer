#pragma once
#include "NodeManager.h"
#include "SessionManager.h"
#include "ndt.pb.h"

namespace ndtapp::status {
class LogicSystem final {
public:
    bool Heartbeat(const ndt::NodeHeartbeat &request) { return nodes_.Report(request); }
    void Route(const ndt::RouteRequest &request, ndt::RouteReply &reply);
    void Validate(const ndt::SessionRequest &request, ndt::SessionReply &reply);
private:
    NodeManager nodes_;
    SessionManager sessions_;
};
}
