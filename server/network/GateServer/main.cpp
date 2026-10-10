#include "CServer.h"
#include "ConfigMgr.h"
#include "server/logic/GateServer/LogicSystem.h"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char **argv) {
    try {
        auto &config = ndtapp::ConfigMgr::Instance();
        config.Load("GateServer", argc, argv);
        const int ioThreads = config.GetInt("ThreadPool", "IOThreads", 4);
        const int logicThreads = config.GetInt("ThreadPool", "LogicThreads", 4);
        if (ioThreads < 1 || ioThreads > 64 || logicThreads < 1 || logicThreads > 64)
            throw std::runtime_error("invalid GateServer thread count");
        ndtapp::LogicThreadPool business(logicThreads,
            config.GetInt("Queue", "LogicCapacity", 128));
        ndtapp::gate::LogicSystem logic;
        boost::asio::io_context io;
        const auto host = config.Get("Server", "Host", "127.0.0.1", "NDT_GATE_HOST");
        const int port = config.GetInt("Server", "Port", 8080, "NDT_GATE_PORT");
        ndtapp::gate::CServer server(io, logic, business, host, uint16_t(port),
            config.GetInt("ThreadPool", "MaxConnections", 64));
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code &, int) { server.Stop(); io.stop(); });
        server.Start();
        std::cout << "GateServer listening on " << host << ':' << port << std::endl;
        std::vector<std::thread> workers;
        for (int i = 1; i < ioThreads; ++i) workers.emplace_back([&] { io.run(); });
        io.run();
        for (auto &worker : workers) worker.join();
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
