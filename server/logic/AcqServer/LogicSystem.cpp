#include "LogicSystem.h"
#include "ConfigMgr.h"
#include "DeviceBridge.h"
#include "FileUploadTask.h"
#include "ScanLogic.h"
#include "ScanTask.h"
#include "NdtCommon.h"
#include "server/network/AcqServer/CSession.h"
#include <iostream>

namespace ndtapp::acq {
LogicSystem::LogicSystem()
    : capacity_(std::size_t(ConfigMgr::Instance().GetInt("Queue", "LogicCapacity", 128))),
      scanPool_(ConfigMgr::Instance().GetInt("ThreadPool", "LogicThreads", 4), capacity_),
      nodeId_(ConfigMgr::Instance().Get("Server", "NodeId", "acq1", "NDT_ACQ_NODE_ID")) {
    if (!capacity_ || capacity_ > 65536) throw std::runtime_error("invalid Acq logic capacity");
    RegisterCallBacks();
}
LogicSystem::~LogicSystem() { Stop(); }
void LogicSystem::Start() {
    dispatchThread_ = std::thread([this] { Dispatch(); });
    heartbeatThread_ = std::thread([this] { HeartbeatLoop(); });
    recoveryThread_ = std::thread([this] { FileUploadTask::Recover(file_); });
}
void LogicSystem::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    queued_.notify_all(); heartbeatWake_.notify_all();
    if (dispatchThread_.joinable()) dispatchThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
    if (recoveryThread_.joinable()) recoveryThread_.join();
    scanPool_.Stop();
}
bool LogicSystem::PostMsgToQue(MsgNode node) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_ || messages_.size() >= capacity_) return false;
    messages_.push_back(std::move(node)); queued_.notify_one(); return true;
}
void LogicSystem::RegisterCallBacks() {
    callbacks_[1] = [this](const MsgNode &node) {
        const auto token = std::string(node.body.as_object().at("token").as_string());
        JwtClaims claims; std::string error;
        if (!jwt_.Verify(token, nodeId_, claims, error)) {
            std::cerr << "Acq JWT: " << error << '\n';
            node.session->Send(8, {{"error","invalid session JWT"}});
            node.session->Close(); return;
        }
        node.session->SetAuthenticated();
        node.session->Send(1, {{"state","authenticated"},
                               {"device_connected",deviceConnected_.load()}});
    };
    callbacks_[2] = [this](const MsgNode &node) {
        const ScanConfig config = ScanLogic::Parse(node.body.as_object());
        if (!node.session->BeginScan()) {
            node.session->Send(8, {{"error","scan already running"}}); return;
        }
        node.session->ResetStop();
        const bool accepted = scanPool_.TryPost([this, session=node.session, config] {
            try { ScanTask::Run(session, config, proc_, file_, deviceConnected_, runningTasks_); }
            catch (const std::exception &e) {
                session->Send(8, {{"error",e.what()}}); session->EndScan();
            }
        });
        if (!accepted) {
            node.session->EndScan();
            node.session->Send(8, {{"error","scan queue full"}});
        }
    };
    callbacks_[3] = [](const MsgNode &node) { node.session->RequestStop(); };
    callbacks_[6] = [this](const MsgNode &node) {
        deviceConnected_ = DeviceBridge::Probe();
        node.session->Send(6, {{"state","device checked"},
                               {"device_connected",deviceConnected_.load()}});
    };
}
void LogicSystem::Dispatch() {
    for (;;) {
        MsgNode node;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queued_.wait(lock, [this] { return stopping_ || !messages_.empty(); });
            if (messages_.empty()) return;
            node = std::move(messages_.front()); messages_.pop_front();
        }
        try {
            if (!node.session->Authenticated() && node.id != 1) {
                node.session->Send(8, {{"error","first frame must be Hello"}});
                node.session->Close();
            } else {
                const auto callback = callbacks_.find(node.id);
                if (callback == callbacks_.end())
                    node.session->Send(8, {{"error","unsupported message type"}});
                else callback->second(node);
            }
        } catch (const std::exception &e) {
            std::cerr << "Acq logic: " << e.what() << '\n';
            node.session->Send(8, {{"error",e.what()}});
        }
        node.session->ResumeRead();
    }
}
void LogicSystem::HeartbeatLoop() {
    auto &cfg = ConfigMgr::Instance();
    const auto id = cfg.Get("Server", "NodeId", "acq1", "NDT_ACQ_NODE_ID");
    const auto host = cfg.Get("Server", "Host", "127.0.0.1", "NDT_ACQ_HOST");
    const int port = cfg.GetInt("Server", "Port", 7001, "NDT_ACQ_PORT");
    for (;;) {
        deviceConnected_ = DeviceBridge::Probe();
        ndt::NodeHeartbeat heartbeat;
        heartbeat.set_node_id(id); heartbeat.set_host(host); heartbeat.set_port(port);
        heartbeat.set_active_connections(activeConnections_);
        heartbeat.set_running_tasks(runningTasks_);
        heartbeat.set_device_connected(deviceConnected_);
        heartbeat.set_available(true); heartbeat.set_heartbeat_timestamp_ms(epochMs());
        ndt::Ack reply; status_.Heartbeat(heartbeat, reply);
        std::unique_lock<std::mutex> lock(heartbeatMutex_);
        if (heartbeatWake_.wait_for(lock, std::chrono::seconds(2), [this] {
            std::lock_guard<std::mutex> state(mutex_); return stopping_;
        })) return;
    }
}
}
