#pragma once
#include "ScanLogic.h"
#include "server/network/AcqServer/ProcGrpcClient.h"
#include "server/network/AcqServer/FileGrpcClient.h"
#include <atomic>
#include <cstdint>
#include <memory>

namespace ndtapp::acq {
class CSession;
class ScanTask final {
public:
    static void Run(std::shared_ptr<CSession> session, ScanConfig config,
                    ProcGrpcClient &proc, FileGrpcClient &file,
                    std::atomic<bool> &deviceConnected,
                    std::atomic<uint32_t> &runningTasks);
};
}
