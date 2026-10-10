#include "ConfigMgr.h"
#include "NdtCommon.h"
#include "RedisConPool.h"
#include "RedisMgr.h"
#include "FileGrpcServiceImpl.h"
#include <iostream>

int main(int argc, char **argv) {
    try {
        ndtapp::ConfigMgr::Instance().Load("FileServer", argc, argv);
        ndtapp::RedisConPool pool;
        ndtapp::RedisMgr redis(pool);
        ndtapp::file::FileManager files;
        ndtapp::file::LogicSystem logic(files, redis);
        ndtapp::file::FileGrpcServiceImpl service(logic);
        const std::string listen = ndtapp::address("NDT_FILE_HOST", "NDT_FILE_PORT", 50054);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        auto server = builder.BuildAndStart();
        if (!server) throw std::runtime_error("FileServer bind failed: " + listen);
        std::cout << "FileServer listening on " << listen << std::endl;
        server->Wait();
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
