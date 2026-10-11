#include "ConfigMgr.h"
#include "NdtCommon.h"
#include "StatusGrpcServiceImpl.h"
#include "server/network/common/GrpcServerRunner.h"
#include <iostream>

int main(int argc, char **argv) {
    try {
        ndtapp::ConfigMgr::Instance().Load("StatusServer", argc, argv);
        ndtapp::status::LogicSystem logic;
        ndtapp::status::StatusGrpcServiceImpl service(logic);
        const std::string listen = ndtapp::address("NDT_STATUS_HOST", "NDT_STATUS_PORT", 50052);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        auto server = builder.BuildAndStart();
        if (!server) throw std::runtime_error("StatusServer bind failed: " + listen);
        std::cout << "StatusServer listening on " << listen << std::endl;
        ndtapp::WaitForGrpcShutdown(*server);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
