#pragma once
#include "server/network/AcqServer/MsgNode.h"
#include "server/network/AcqServer/StatusGrpcClient.h"
#include "server/network/AcqServer/ProcGrpcClient.h"
#include "server/network/AcqServer/FileGrpcClient.h"
#include "Jwt.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ndtapp::acq {
class LogicSystem final {
public:
    LogicSystem();
    ~LogicSystem();
    LogicSystem(const LogicSystem &) = delete;
    LogicSystem &operator=(const LogicSystem &) = delete;
    void Start();
    void Stop();
    bool PostMsgToQue(MsgNode node);
    void OnConnect() { ++activeConnections_; }
    void OnDisconnect() { --activeConnections_; }
private:
    struct ScanWorker {
        std::shared_ptr<CSession> session;
        std::shared_ptr<std::atomic<bool>> finished;
        std::thread thread;
    };
    using Callback = std::function<void(const MsgNode &)>;
    void RegisterCallBacks();
    void Dispatch();
    void ReapFinishedScans();
    bool HasCompletedScan() const;
    void HeartbeatLoop();
    std::map<uint16_t, Callback> callbacks_;
    std::mutex mutex_;
    std::condition_variable queued_;
    std::deque<MsgNode> messages_;
    std::size_t capacity_;
    std::size_t maxConcurrentScans_;
    bool stopping_ = false;
    std::thread dispatchThread_, heartbeatThread_, recoveryThread_;
    // Only the dispatch thread changes this vector; Stop joins it first.
    std::vector<ScanWorker> scanWorkers_;
    std::condition_variable heartbeatWake_;
    std::mutex heartbeatMutex_;
    StatusGrpcClient status_;
    ProcGrpcClient proc_;
    FileGrpcClient file_;
    Jwt jwt_;
    std::string nodeId_;
    std::atomic<uint32_t> activeConnections_{0}, runningTasks_{0};
    std::atomic<bool> deviceConnected_{false};
};
}
