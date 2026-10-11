#include "LogicSystem.h"

namespace ndtapp::status {
void LogicSystem::Route(const ndt::RouteRequest &request, ndt::RouteReply &reply) {
    if (!request.user_id()) { reply.set_error("invalid user"); return; }
    const auto node = nodes_.Select();
    if (!node) { reply.set_error("no healthy AcqServer"); return; }
    reply.set_ok(true); reply.set_host(node->host); reply.set_port(node->port);
    reply.set_node_id(node->id);
}
}
