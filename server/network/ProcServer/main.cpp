#include "ConfigMgr.h"
#include "NdtCommon.h"
#include "ProcGrpcServiceImpl.h"
#include "server/network/common/GrpcServerRunner.h"
#include <iostream>

int main(int argc, char **argv) {
    try {
        ndtapp::ConfigMgr::Instance().Load("ProcServer", argc, argv);
        ndtapp::proc::LogicSystem logic;
        ndtapp::proc::ProcGrpcServiceImpl service(logic);
        const std::string listen = ndtapp::address("NDT_PROC_HOST", "NDT_PROC_PORT", 50053);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        auto server = builder.BuildAndStart();
        if (!server) throw std::runtime_error("ProcServer bind failed: " + listen);
        std::cout << "ProcServer listening on " << listen << std::endl;
        ndtapp::WaitForGrpcShutdown(*server);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
