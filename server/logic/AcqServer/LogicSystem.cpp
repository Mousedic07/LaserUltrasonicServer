#include "LogicSystem.h"
#include "ConfigMgr.h"
#include "DeviceBridge.h"
#include "FileUploadTask.h"
#include "ScanLogic.h"
#include "ScanTask.h"
#include "NdtCommon.h"
#include "server/network/AcqServer/CSession.h"
#include <iostream>
#include <stdexcept>

namespace ndtapp::acq {
LogicSystem::LogicSystem()
    : capacity_(std::size_t(ConfigMgr::Instance().GetInt("Queue", "LogicCapacity", 128))),
      maxConcurrentScans_(std::size_t(ConfigMgr::Instance().GetInt(
          "Scan", "MaxConcurrent", 4, "NDT_MAX_CONCURRENT_SCANS"))),
      nodeId_(ConfigMgr::Instance().Get("Server", "NodeId", "acq1", "NDT_ACQ_NODE_ID")) {
    if (!capacity_ || capacity_ > 65536) throw std::runtime_error("invalid Acq logic capacity");
    if (!maxConcurrentScans_ || maxConcurrentScans_ > 64)
        throw std::runtime_error("invalid concurrent scan limit");
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
        messages_.clear();
    }
    queued_.notify_all();
    heartbeatWake_.notify_all();
    if (dispatchThread_.joinable()) dispatchThread_.join();
    for (auto &scan : scanWorkers_) scan.session->RequestStop();
    for (auto &scan : scanWorkers_) if (scan.thread.joinable()) scan.thread.join();
    scanWorkers_.clear();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
    if (recoveryThread_.joinable()) recoveryThread_.join();
}
bool LogicSystem::PostMsgToQue(MsgNode node) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_ || messages_.size() >= capacity_) return false;
    messages_.push_back(std::move(node));
    queued_.notify_one();
    return true;
}
bool LogicSystem::HasCompletedScan() const {
    for (const auto &scan : scanWorkers_)
        if (scan.finished->load()) return true;
    return false;
}
void LogicSystem::ReapFinishedScans() {
    for (auto current = scanWorkers_.begin(); current != scanWorkers_.end();) {
        if (!current->finished->load()) { ++current; continue; }
        if (current->thread.joinable()) current->thread.join();
        current = scanWorkers_.erase(current);
    }
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
        if (node.session->IsClosed()) return;
        const ScanConfig config = ScanLogic::Parse(node.body.as_object());
        ReapFinishedScans();
        if (scanWorkers_.size() >= maxConcurrentScans_) {
            node.session->Send(8, {{"error","concurrent scan limit reached"}});
            return;
        }
        auto finished = std::make_shared<std::atomic<bool>>(false);
        if (!node.session->BeginScan()) {
            node.session->Send(8, {{"error","scan already running"}});
            return;
        }
        node.session->ResetStop();
        try { scanWorkers_.push_back({node.session, finished, {}}); }
        catch (...) { node.session->EndScan(); throw; }
        try {
            scanWorkers_.back().thread = std::thread([this, session=node.session, config, finished] {
                try { ScanTask::Run(session, config, proc_, file_, deviceConnected_, runningTasks_); }
                catch (const std::exception &error) {
                    session->Send(8, {{"error",error.what()}});
                    session->EndScan();
                } catch (...) {
                    session->Send(8, {{"error","scan worker failed"}});
                    session->EndScan();
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    finished->store(true);
                }
                queued_.notify_one();
            });
        } catch (...) {
            scanWorkers_.pop_back();
            node.session->EndScan();
            throw;
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
        ReapFinishedScans();
        MsgNode node;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queued_.wait(lock, [this] {
                return stopping_ || !messages_.empty() || HasCompletedScan();
            });
            if (stopping_) return;
            if (messages_.empty()) continue;
            node = std::move(messages_.front());
            messages_.pop_front();
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
        } catch (const std::exception &error) {
            std::cerr << "Acq logic: " << error.what() << '\n';
            node.session->Send(8, {{"error",error.what()}});
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
