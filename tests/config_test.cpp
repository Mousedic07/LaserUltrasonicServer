#include "ConfigMgr.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    try {
        auto &config = ndtapp::ConfigMgr::Instance();
        config.Load("GateServer", argc, argv);
        if (config.Path().empty() || config.Required("Server", "Host") != "127.0.0.1" ||
            config.GetInt("Server", "Port", 0) != 8080 ||
            config.GetDouble("Test", "Missing", 1.25) != 1.25 ||
            !config.GetBool("Test", "Missing", true) ||
            config.Legacy("NDT_AUTH_PORT", "0") != "50051")
            throw std::runtime_error("ConfigMgr did not parse example INI");
        _putenv_s("NDT_GATE_PORT", "18080");
        if (config.GetInt("Server", "Port", 0, "NDT_GATE_PORT") != 18080 ||
            config.Legacy("NDT_GATE_PORT", "0") != "18080")
            throw std::runtime_error("ConfigMgr override failed");
        const auto root = std::filesystem::path(config.Path()).parent_path().parent_path();
        for (const auto &entry : {std::pair{"AuthServer", 50051},
                                  std::pair{"StatusServer", 50052},
                                  std::pair{"AcqServer", 7001},
                                  std::pair{"ProcServer", 50053},
                                  std::pair{"FileServer", 50054}}) {
            std::string path = (root / entry.first / "config.example.ini").string();
            char program[] = "config_test", option[] = "--config";
            char *args[] = {program, option, &path[0]};
            config.Load(entry.first, 3, args);
            if (config.Required("Server", "Host") != "127.0.0.1" ||
                config.GetInt("Server", "Port", 0) != entry.second)
                throw std::runtime_error(std::string("ConfigMgr failed for ") + entry.first);
        }
        std::cout << "All six ConfigMgr INIs, typed default and environment override passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
