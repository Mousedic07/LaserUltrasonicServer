#include "LogicSystem.h"

namespace ndtapp::status {
void LogicSystem::Route(const ndt::RouteRequest &request, ndt::RouteReply &reply) {
    if (!request.user_id()) { reply.set_error("invalid user"); return; }
    const auto node = nodes_.Select();
    if (!node) { reply.set_error("no healthy AcqServer"); return; }
    reply.set_ok(true); reply.set_host(node->host); reply.set_port(node->port);
    reply.set_node_id(node->id); reply.set_token(sessions_.Issue(request.user_id()));
}
void LogicSystem::Validate(const ndt::SessionRequest &request, ndt::SessionReply &reply) {
    const auto user = sessions_.Validate(request.token());
    if (!user) { reply.set_error("session expired or invalid"); return; }
    reply.set_ok(true); reply.set_user_id(user);
}
}
