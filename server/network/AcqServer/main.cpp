#include "CServer.h"
#include "ConfigMgr.h"
#include "server/logic/AcqServer/LogicSystem.h"
#include <boost/asio/signal_set.hpp>
#include <iostream>

int main(int argc, char **argv) {
    try {
        auto &config = ndtapp::ConfigMgr::Instance();
        config.Load("AcqServer", argc, argv);
        const int ioThreads = config.GetInt("ThreadPool", "IOThreads", 4);
        const int maxConnections = config.GetInt("ThreadPool", "MaxConnections", 128);
        const int port = config.GetInt("Server", "Port", 7001, "NDT_ACQ_PORT");
        if (ioThreads < 1 || ioThreads > 64 || maxConnections < 1 || maxConnections > 65536 ||
            port < 1 || port > 65535) throw std::runtime_error("invalid AcqServer limits");
        AsioIOServicePool ioPool(ioThreads);
        ndtapp::acq::LogicSystem logic;
        boost::asio::io_context acceptIo;
        const auto host = config.Get("Server", "Host", "127.0.0.1", "NDT_ACQ_HOST");
        ndtapp::acq::CServer server(acceptIo, ioPool, logic, host, uint16_t(port), maxConnections);
        boost::asio::signal_set signals(acceptIo, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code &, int) { server.Stop(); acceptIo.stop(); });
        logic.Start(); server.Start();
        std::cout << "AcqServer listening on " << host << ':' << port << std::endl;
        acceptIo.run();
        server.Stop(); logic.Stop(); ioPool.Stop();
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
