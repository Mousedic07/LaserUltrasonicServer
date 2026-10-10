#include "ConfigMgr.h"
#include "MySqlPool.h"
#include "NdtCommon.h"
#include "AuthGrpcServiceImpl.h"
#include <iostream>

int main(int argc, char **argv) {
    try {
        ndtapp::ConfigMgr::Instance().Load("AuthServer", argc, argv);
        ndtapp::MySqlPool pool;
        ndtapp::auth::LogicSystem logic(pool);
        ndtapp::auth::AuthGrpcServiceImpl service(logic);
        const std::string listen = ndtapp::address("NDT_AUTH_HOST", "NDT_AUTH_PORT", 50051);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        auto server = builder.BuildAndStart();
        if (!server) throw std::runtime_error("AuthServer bind failed: " + listen);
        std::cout << "AuthServer listening on " << listen << std::endl;
        server->Wait();
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
