#pragma once
#include "server/logic/StatusServer/LogicSystem.h"
#include "ndt.grpc.pb.h"

namespace ndtapp::status {
class StatusGrpcServiceImpl final : public ndt::StatusService::Service {
public:
    explicit StatusGrpcServiceImpl(LogicSystem &logic) : logic_(logic) {}
    grpc::Status Heartbeat(grpc::ServerContext *, const ndt::NodeHeartbeat *, ndt::Ack *) override;
    grpc::Status Route(grpc::ServerContext *, const ndt::RouteRequest *, ndt::RouteReply *) override;
    grpc::Status Validate(grpc::ServerContext *, const ndt::SessionRequest *, ndt::SessionReply *) override;
private:
    LogicSystem &logic_;
};
}
