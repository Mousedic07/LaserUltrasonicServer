#pragma once
#include <grpcpp/server.h>

namespace ndtapp {
// Wait on a gRPC server and drain active RPCs after a console stop signal.
void WaitForGrpcShutdown(grpc::Server &server);
}
