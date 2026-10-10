#include "ScanTask.h"
#include "DeviceBridge.h"
#include "FftTask.h"
#include "FileUploadTask.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include "server/network/AcqServer/CSession.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

namespace ndtapp::acq {
namespace fs = std::filesystem;
void ScanTask::Run(std::shared_ptr<CSession> session, ScanConfig config,
                   ProcGrpcClient &proc, FileGrpcClient &file,
                   std::atomic<bool> &deviceConnected,
                   std::atomic<uint32_t> &runningTasks) {
    ++runningTasks;
    const std::string scanId = randomHex(12);
    const fs::path spoolDir = ConfigMgr::Instance().Get("Storage", "SpoolRoot", "spool", "NDT_SPOOL_ROOT");
    fs::create_directories(spoolDir);
    const fs::path spool = spoolDir / (scanId + ".spool");
    auto &cfg = ConfigMgr::Instance();
    BoundedQueue<ndt::Waveform> fftQueue(cfg.GetInt("Queue", "FftCapacity", 8));
    BoundedQueue<FileChunk> fileQueue(cfg.GetInt("Queue", "FileCapacity", 8));
    std::atomic<bool> complete{false};
    std::thread fftWorker(FftTask::Run, session, std::ref(fftQueue), std::ref(proc));
    std::thread fileWorker(FileUploadTask::Run, session, std::ref(fileQueue), std::ref(file),
                           scanId, config.totalBytes, spool, std::cref(complete));
    uint64_t offset = 0, frameId = 0;
    uint32_t completed = 0;
    try {
        DeviceBridge device;
        deviceConnected = true;
        device.Configure(config);
        std::ofstream out(spool, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot open spool file");
        session->Send(6, {{"state","running"},{"scan_id",scanId},
                          {"completed_points",0},{"total_points",config.points},
                          {"device_connected",true}});
        for (uint32_t point = 0; point < config.points && !session->StopRequested(); ++point) {
            const double x = point * config.step;
            auto frames = device.Acquire(point, x);
            if (frames.size() != config.channels) throw std::runtime_error("DAQ channel count changed");
            for (auto &value : frames) {
                if (session->StopRequested()) break;
                auto &frame = value.as_object();
                frame["scan_id"] = scanId; frame["frame_id"] = frameId;
                const std::string record = ScanLogic::EncodeRecord(frameId, frame, config.samples);
                out.write(record.data(), std::streamsize(record.size())); out.flush();
                if (!out) throw std::runtime_error("spool write failed");
                session->Send(4, frame);
                ndt::Waveform wave;
                wave.set_scan_id(scanId); wave.set_frame_id(frameId);
                wave.set_channel_id(uint32_t(ScanLogic::Nonnegative(frame,"channel_id")));
                wave.set_timestamp_ms(int64_t(ScanLogic::Positive(frame,"timestamp_ms")));
                wave.set_position_x_mm(x); wave.set_position_y_mm(0);
                wave.set_sample_rate_hz(config.rate);
                for (const auto &sample : frame.at("samples").as_array())
                    wave.add_samples(float(sample.as_double()));
                fftQueue.push(std::move(wave));
                for (std::size_t i = 0; i < record.size(); i += 65536) {
                    const auto len = std::min<std::size_t>(65536, record.size() - i);
                    fileQueue.push(FileChunk{offset + i, record.substr(i, len)});
                }
                offset += record.size(); ++frameId;
            }
            ++completed;
            session->Send(6, {{"state","running"},{"scan_id",scanId},
                              {"completed_points",completed},{"total_points",config.points}});
        }
        device.Stop(); out.close();
        if (!session->StopRequested() && completed == config.points && offset == config.totalBytes) {
            const std::string hash = sha256File(spool.string());
            {
                std::ofstream marker(spoolDir / (scanId + ".ready"), std::ios::trunc);
                marker << config.totalBytes << ' ' << hash << '\n'; marker.flush();
                if (!marker) throw std::runtime_error("cannot persist upload recovery marker");
            }
            complete = true;
        }
    } catch (const std::exception &e) {
        deviceConnected = false;
        session->Send(8, {{"error",e.what()}});
    }
    fftQueue.close(); fileQueue.close();
    fftWorker.join(); fileWorker.join();
    session->Send(6, {{"state",complete ? "complete" : "stopped"},
                      {"scan_id",scanId},{"completed_points",completed},
                      {"total_points",config.points}});
    --runningTasks;
    session->EndScan();
}
}
