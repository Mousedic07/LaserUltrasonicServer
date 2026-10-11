#include "StatusGrpcServiceImpl.h"

namespace ndtapp::status {
grpc::Status StatusGrpcServiceImpl::Heartbeat(grpc::ServerContext *,
    const ndt::NodeHeartbeat *request, ndt::Ack *reply) {
    reply->set_ok(logic_.Heartbeat(*request));
    if (!reply->ok()) reply->set_error("invalid heartbeat");
    return grpc::Status::OK;
}
grpc::Status StatusGrpcServiceImpl::Route(grpc::ServerContext *,
    const ndt::RouteRequest *request, ndt::RouteReply *reply) {
    logic_.Route(*request, *reply); return grpc::Status::OK;
}
}
