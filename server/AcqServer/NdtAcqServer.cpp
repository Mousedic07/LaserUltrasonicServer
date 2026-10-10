#include "NdtCommon.h"
#include "AsioIOServicePool.h"
#include "ndt.grpc.pb.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <winsock2.h>

namespace {
namespace fs = std::filesystem;
using ndtapp::Json;
using ndtapp::tcp;
std::atomic<uint32_t> activeConnections{0}, runningTasks{0};
std::atomic<bool> deviceConnected{false};

double number(const boost::json::object &obj, const char *key) {
    const Json &v = obj.at(key);
    if (v.is_double()) return v.as_double();
    if (v.is_int64()) return double(v.as_int64());
    if (v.is_uint64()) return double(v.as_uint64());
    throw std::invalid_argument(std::string("non-numeric ") + key);
}
uint64_t positiveInt(const boost::json::object &obj, const char *key) {
    const double value = number(obj, key);
    if (value < 1 || value != std::floor(value)) throw std::invalid_argument(key);
    return uint64_t(value);
}
uint64_t nonnegativeInt(const boost::json::object &obj, const char *key) {
    const double value = number(obj, key);
    if (value < 0 || value != std::floor(value)) throw std::invalid_argument(key);
    return uint64_t(value);
}
struct ScanConfig {
    double rate = 0, excitation = 0, step = 0;
    uint32_t samples = 0, points = 0, channels = 0;
    uint64_t totalBytes = 0;
};
ScanConfig parseConfig(const boost::json::object &obj) {
    ScanConfig c;
    c.rate = number(obj, "sample_rate_hz");
    c.excitation = number(obj, "excitation_frequency_hz");
    c.step = number(obj, "scan_step_mm");
    c.samples = uint32_t(positiveInt(obj, "sample_count"));
    c.points = uint32_t(positiveInt(obj, "scan_points"));
    c.channels = uint32_t(positiveInt(obj, "channel_count"));
    if (!std::isfinite(c.rate) || !std::isfinite(c.excitation) || !std::isfinite(c.step) ||
        c.rate <= c.excitation * 2 || c.excitation <= 0 || c.step <= 0 ||
        c.samples < 64 || c.samples > 16384 || c.points > 10000 || c.channels > 16)
        throw std::invalid_argument("invalid scan config or Nyquist violation");
    c.totalBytes = uint64_t(c.points) * c.channels * (52 + uint64_t(c.samples) * 4);
    if (c.totalBytes > uint64_t(4) * 1024 * 1024 * 1024)
        throw std::invalid_argument("scan file exceeds 4 GiB limit");
    return c;
}
void setSocketTimeout(tcp::socket &socket) {
    DWORD ms = 5000;
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&ms), sizeof(ms));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&ms), sizeof(ms));
}
class DeviceBridge {
public:
    DeviceBridge() : laser_(io_), stage_(io_), daq_(io_) {
        const int base = ndtapp::envInt("NDT_DEVICE_BASE_PORT", 9000);
        const auto host = boost::asio::ip::make_address(ndtapp::env("NDT_DEVICE_HOST", "127.0.0.1"));
        laser_.connect({host, uint16_t(base)}); setSocketTimeout(laser_);
        stage_.connect({host, uint16_t(base + 1)}); setSocketTimeout(stage_);
        daq_.connect({host, uint16_t(base + 2)}); setSocketTimeout(daq_);
    }
    boost::json::object ask(tcp::socket &socket, const boost::json::object &command) {
        ndtapp::writeLine(socket, command);
        auto &buffer = (&socket == &laser_) ? laserBuffer_ : ((&socket == &stage_) ? stageBuffer_ : daqBuffer_);
        boost::asio::read_until(socket, buffer, '\n');
        std::istream input(&buffer);
        std::string line;
        std::getline(input, line);
        auto reply = ndtapp::parseJson(line).as_object();
        if (!reply.at("ok").as_bool())
            throw std::runtime_error(std::string(reply.at("error").as_string()));
        return reply;
    }
    void configure(const ScanConfig &config) {
        ask(laser_, {{"command","connect"}});
        ask(laser_, {{"command","set_frequency"},{"frequency_hz",config.excitation}});
        ask(stage_, {{"command","set_step"},{"step_mm",config.step}});
        ask(daq_, {{"command","configure"},{"sample_rate_hz",config.rate},
            {"sample_count",config.samples},{"channel_count",config.channels},
            {"excitation_frequency_hz",config.excitation}});
        ask(daq_, {{"command","start_acquisition"}});
    }
    boost::json::array acquire(uint32_t point, double x) {
        ask(stage_, {{"command","move_to"},{"x_mm",x},{"y_mm",0}});
        ask(laser_, {{"command","trigger"}});
        return ask(daq_, {{"command","read_waveform"},{"point",point},{"x_mm",x},{"y_mm",0}})
            .at("frames").as_array();
    }
    void stop() {
        try { ask(daq_, {{"command","stop_acquisition"}}); } catch (...) {}
        try { ask(laser_, {{"command","stop"}}); } catch (...) {}
    }
private:
    boost::asio::io_context io_;
    tcp::socket laser_, stage_, daq_;
    boost::asio::streambuf laserBuffer_{16 * 1024 * 1024};
    boost::asio::streambuf stageBuffer_{16 * 1024 * 1024};
    boost::asio::streambuf daqBuffer_{16 * 1024 * 1024};
};
bool probeDevices() {
    try { DeviceBridge bridge; return true; } catch (...) { return false; }
}
struct FileChunk { uint64_t offset = 0; std::string bytes; };
struct Session : std::enable_shared_from_this<Session> {
    explicit Session(tcp::socket socket) : socket(std::move(socket)) {}
    void send(uint16_t type, const Json &value) {
        std::lock_guard<std::mutex> lock(sendMutex);
        ndtapp::sendFrame(socket, type, value);
    }
    tcp::socket socket;
    std::mutex sendMutex;
    std::atomic<bool> stop{false};
    std::atomic<bool> scanRunning{false};
    std::thread scan;
};
void sendSafe(const std::shared_ptr<Session> &session, uint16_t type, const Json &value) {
    try { session->send(type, value); } catch (...) { session->stop = true; }
}

void appendLittle(std::string &out, uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(char(value >> (i * 8)));
}
void appendDouble(std::string &out, double value) {
    static_assert(sizeof(value) == 8, "double must be 64-bit");
    uint64_t bits; std::memcpy(&bits, &value, 8); appendLittle(out, bits, 8);
}
std::string encodeRecord(uint64_t frameId, const boost::json::object &frame, uint32_t sampleCount) {
    std::string out("WAV1", 4);
    appendLittle(out, frameId, 8);
    appendLittle(out, nonnegativeInt(frame, "channel_id"), 4);
    appendLittle(out, positiveInt(frame, "timestamp_ms"), 8);
    appendDouble(out, number(frame, "position_x_mm"));
    appendDouble(out, number(frame, "position_y_mm"));
    appendDouble(out, number(frame, "sample_rate_hz"));
    appendLittle(out, sampleCount, 4);
    const auto &samples = frame.at("samples").as_array();
    if (samples.size() != sampleCount) throw std::runtime_error("DAQ sample count changed");
    for (const auto &sample : samples) {
        float value = float(sample.is_double() ? sample.as_double() : double(sample.as_int64()));
        uint32_t bits; std::memcpy(&bits, &value, 4); appendLittle(out, bits, 4);
    }
    return out;
}
void processTask(std::shared_ptr<Session> session, ndtapp::BoundedQueue<ndt::Waveform> &queue) {
    auto stub = ndt::ProcessService::NewStub(grpc::CreateChannel(
        ndtapp::address("NDT_PROC_HOST","NDT_PROC_PORT",50053), grpc::InsecureChannelCredentials()));
    ndt::Waveform wave;
    while (queue.pop(wave)) {
        ndt::Spectrum spectrum;
        grpc::ClientContext context; ndtapp::deadline(context, 5000);
        const auto status = stub->Fft(&context, wave, &spectrum);
        if (!status.ok()) {
            sendSafe(session, 8, {{"error","ProcServer: " + status.error_message()}});
            continue;
        }
        boost::json::array bins, amplitudes;
        for (double x : spectrum.frequency_bins_hz()) bins.push_back(x);
        for (double y : spectrum.amplitude_spectrum()) amplitudes.push_back(y);
        sendSafe(session, 5, {{"scan_id",spectrum.scan_id()}, {"frame_id",spectrum.frame_id()},
            {"channel_id",spectrum.channel_id()}, {"frequency_bins_hz",std::move(bins)},
            {"amplitude_spectrum",std::move(amplitudes)},
            {"dominant_frequency_hz",spectrum.dominant_frequency_hz()}});
    }
}
void uploadTask(std::shared_ptr<Session> session, ndtapp::BoundedQueue<FileChunk> &queue,
                const std::string &scanId, uint64_t total, const fs::path &spool,
                const std::atomic<bool> &scanComplete) {
    auto stub = ndt::ArchiveService::NewStub(grpc::CreateChannel(
        ndtapp::address("NDT_FILE_HOST","NDT_FILE_PORT",50054), grpc::InsecureChannelCredentials()));
    ndt::UploadInfo info; info.set_upload_id(scanId); info.set_total_bytes(total);
    uint64_t confirmed = 0;
    auto query = [&]() -> bool {
        ndt::UploadProgress progress;
        grpc::ClientContext context; ndtapp::deadline(context);
        auto status = stub->Query(&context, info, &progress);
        if (!status.ok() || !progress.ok()) return false;
        confirmed = progress.confirmed_bytes(); return true;
    };
    if (!query()) { sendSafe(session, 7, {{"status","FileServer unavailable"}}); return; }
    FileChunk chunk;
    while (queue.pop(chunk)) {
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (confirmed >= chunk.offset + chunk.bytes.size()) break;
            if (confirmed < chunk.offset && !query()) { std::this_thread::sleep_for(std::chrono::milliseconds(500)); continue; }
            const std::size_t skip = std::size_t(confirmed - chunk.offset);
            ndt::UploadChunk request; *request.mutable_info() = info;
            request.set_offset(confirmed); request.set_data(chunk.bytes.data() + skip, chunk.bytes.size() - skip);
            ndt::UploadProgress progress;
            grpc::ClientContext context; ndtapp::deadline(context, 5000);
            const auto status = stub->Append(&context, request, &progress);
            if (status.ok() && progress.ok()) {
                confirmed = progress.confirmed_bytes();
                const double percent = 100.0 * double(confirmed) / double(total);
                sendSafe(session, 7, {{"filename",progress.filename()}, {"percent",percent},
                    {"speed_bytes_per_sec",progress.speed_bytes_per_sec()},{"status",progress.status()}});
            } else {
                query();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        if (confirmed < chunk.offset + chunk.bytes.size()) {
            sendSafe(session, 7, {{"status","upload failed after retries"}}); return;
        }
    }
    if (!scanComplete || confirmed != total) {
        sendSafe(session, 7, {{"status","partial scan retained for recovery"}}); return;
    }
    info.set_sha256(ndtapp::sha256File(spool.string()));
    ndt::UploadProgress progress;
    grpc::ClientContext context; ndtapp::deadline(context, 30000);
    const auto status = stub->Complete(&context, info, &progress);
    if (!status.ok() || !progress.ok())
        sendSafe(session, 7, {{"status","archive validation failed"}});
    else {
        std::error_code ignored;
        fs::remove(spool.parent_path() / (scanId + ".ready"), ignored);
        sendSafe(session, 7, {{"filename",progress.filename()},{"percent",100.0},
                              {"speed_bytes_per_sec",0.0},{"status","complete"}});
    }
}

void recoverReadyUploads() {
  try {
    const fs::path root = ndtapp::env("NDT_SPOOL_ROOT", "spool");
    fs::create_directories(root);
    auto stub = ndt::ArchiveService::NewStub(grpc::CreateChannel(
        ndtapp::address("NDT_FILE_HOST","NDT_FILE_PORT",50054), grpc::InsecureChannelCredentials()));
    static const std::regex validId("^[0-9a-f]{24}$");
    for (const auto &entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ready") continue;
        const std::string id = entry.path().stem().string();
        if (!std::regex_match(id, validId)) continue;
        const fs::path spool = root / (id + ".spool");
        try {
            uint64_t total = 0;
            std::string hash;
            std::ifstream marker(entry.path()); marker >> total >> hash;
            if (!marker || hash.size() != 64 || !fs::exists(spool) || fs::file_size(spool) != total)
                throw std::runtime_error("invalid ready marker or spool size");
            marker.close();
            if (ndtapp::sha256File(spool.string()) != hash)
                throw std::runtime_error("spool SHA-256 changed");
            ndt::UploadInfo info; info.set_upload_id(id); info.set_total_bytes(total); info.set_sha256(hash);
            ndt::UploadProgress progress;
            grpc::ClientContext queryContext; ndtapp::deadline(queryContext, 5000);
            const auto queried = stub->Query(&queryContext, info, &progress);
            if (!queried.ok() || !progress.ok()) throw std::runtime_error("FileServer Query failed");
            uint64_t offset = progress.confirmed_bytes();
            std::ifstream source(spool, std::ios::binary);
            std::array<char, 65536> data{};
            while (offset < total) {
                source.seekg(std::streamoff(offset));
                const std::size_t length = std::size_t(std::min<uint64_t>(data.size(), total - offset));
                source.read(data.data(), std::streamsize(length));
                if (source.gcount() != std::streamsize(length)) throw std::runtime_error("spool read failed");
                ndt::UploadChunk chunk; *chunk.mutable_info() = info;
                chunk.set_offset(offset); chunk.set_data(data.data(), length);
                ndt::UploadProgress appended;
                grpc::ClientContext appendContext; ndtapp::deadline(appendContext, 5000);
                const auto status = stub->Append(&appendContext, chunk, &appended);
                if (!status.ok() || !appended.ok() || appended.confirmed_bytes() <= offset)
                    throw std::runtime_error("FileServer Append failed");
                offset = appended.confirmed_bytes();
            }
            ndt::UploadProgress done;
            grpc::ClientContext completeContext; ndtapp::deadline(completeContext, 30000);
            const auto finished = stub->Complete(&completeContext, info, &done);
            if (!finished.ok() || !done.ok()) throw std::runtime_error("FileServer Complete failed");
            fs::remove(entry.path());
            std::cout << "Recovered upload " << id << " from disk" << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Upload recovery " << id << ": " << e.what() << '\n';
        }
    }
  } catch (const std::exception &e) {
      std::cerr << "Upload recovery setup: " << e.what() << '\n';
  }
}

void scanTask(std::shared_ptr<Session> session, ScanConfig config) {
    ++runningTasks;
    const std::string scanId = ndtapp::randomHex(12);
    const fs::path spoolDir = ndtapp::env("NDT_SPOOL_ROOT", "spool");
    fs::create_directories(spoolDir);
    const fs::path spool = spoolDir / (scanId + ".spool");
    ndtapp::BoundedQueue<ndt::Waveform> fftQueue(8);
    ndtapp::BoundedQueue<FileChunk> fileQueue(8);
    std::atomic<bool> complete{false};
    std::thread fftWorker(processTask, session, std::ref(fftQueue));
    std::thread fileWorker(uploadTask, session, std::ref(fileQueue), scanId,
                           config.totalBytes, spool, std::cref(complete));
    uint64_t offset = 0, frameId = 0;
    uint32_t completed = 0;
    try {
        DeviceBridge device;
        deviceConnected = true;
        device.configure(config);
        std::ofstream out(spool, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot open spool file");
        sendSafe(session, 6, {{"state","running"},{"scan_id",scanId},
                              {"completed_points",0},{"total_points",config.points},
                              {"device_connected",true}});
        for (uint32_t point = 0; point < config.points && !session->stop; ++point) {
            const double x = point * config.step;
            auto frames = device.acquire(point, x);
            if (frames.size() != config.channels) throw std::runtime_error("DAQ channel count changed");
            for (auto &value : frames) {
                if (session->stop) break;
                auto &frame = value.as_object();
                frame["scan_id"] = scanId;
                frame["frame_id"] = frameId;
                std::string record = encodeRecord(frameId, frame, config.samples);
                out.write(record.data(), std::streamsize(record.size())); out.flush();
                if (!out) throw std::runtime_error("spool write failed");
                sendSafe(session, 4, frame);
                ndt::Waveform wave;
                wave.set_scan_id(scanId); wave.set_frame_id(frameId);
                wave.set_channel_id(uint32_t(nonnegativeInt(frame,"channel_id")));
                wave.set_timestamp_ms(int64_t(positiveInt(frame,"timestamp_ms")));
                wave.set_position_x_mm(x); wave.set_position_y_mm(0);
                wave.set_sample_rate_hz(config.rate);
                for (const auto &sample : frame.at("samples").as_array())
                    wave.add_samples(float(sample.as_double()));
                fftQueue.push(std::move(wave));
                for (std::size_t i = 0; i < record.size(); i += 65536) {
                    const std::size_t len = std::min<std::size_t>(65536, record.size() - i);
                    fileQueue.push(FileChunk{offset + i, record.substr(i, len)});
                }
                offset += record.size(); ++frameId;
            }
            ++completed;
            sendSafe(session, 6, {{"state","running"},{"scan_id",scanId},
                                  {"completed_points",completed},{"total_points",config.points}});
        }
        device.stop(); out.close();
        if (!session->stop && completed == config.points && offset == config.totalBytes) {
            const std::string hash = ndtapp::sha256File(spool.string());
            std::ofstream marker(spoolDir / (scanId + ".ready"), std::ios::trunc);
            marker << config.totalBytes << ' ' << hash << '\n'; marker.flush();
            if (!marker) throw std::runtime_error("cannot persist upload recovery marker");
            complete = true;
        }
    } catch (const std::exception &e) {
        deviceConnected = false;
        sendSafe(session, 8, {{"error",e.what()}});
    }
    fftQueue.close(); fileQueue.close();
    fftWorker.join(); fileWorker.join();
    sendSafe(session, 6, {{"state",complete ? "complete" : "stopped"},
                          {"scan_id",scanId},{"completed_points",completed},
                          {"total_points",config.points}});
    --runningTasks;
    session->scanRunning = false;
}
void clientTask(tcp::socket socket, ndt::StatusService::Stub *status) {
    auto session = std::make_shared<Session>(std::move(socket));
    try {
        uint16_t type; Json value;
        if (!ndtapp::readFrame(session->socket, type, value) || type != 1)
            throw std::runtime_error("first frame must be Hello");
        ndt::SessionRequest request; request.set_token(std::string(value.as_object().at("token").as_string()));
        ndt::SessionReply reply;
        grpc::ClientContext context; ndtapp::deadline(context);
        auto verified = status->Validate(&context, request, &reply);
        if (!verified.ok() || !reply.ok()) throw std::runtime_error("invalid session token");
        session->send(1, {{"state","authenticated"},{"device_connected",deviceConnected.load()}});
        while (ndtapp::readFrame(session->socket, type, value)) {
            if (type == 2) {
                if (session->scan.joinable()) {
                    if (session->scanRunning) {
                        sendSafe(session, 8, {{"error","scan already running"}}); continue;
                    }
                    session->scan.join();
                }
                try {
                    const ScanConfig config = parseConfig(value.as_object());
                    session->stop = false;
                    session->scanRunning = true;
                    session->scan = std::thread(scanTask, session, config);
                } catch (const std::exception &e) { sendSafe(session, 8, {{"error",e.what()}}); }
            } else if (type == 3) {
                session->stop = true;
            } else if (type == 6) {
                deviceConnected = probeDevices();
                sendSafe(session, 6, {{"state","device checked"},
                                      {"device_connected",deviceConnected.load()}});
            } else sendSafe(session, 8, {{"error","unsupported message type"}});
        }
    } catch (const std::exception &e) { std::cerr << "Acq session: " << e.what() << '\n'; }
    session->stop = true;
    if (session->scan.joinable()) session->scan.join();
    --activeConnections;
}
void heartbeatTask(ndt::StatusService::Stub *stub) {
    const std::string id = ndtapp::env("NDT_ACQ_NODE_ID", "acq1");
    const std::string host = ndtapp::env("NDT_ACQ_HOST", "127.0.0.1");
    const int port = ndtapp::envInt("NDT_ACQ_PORT", 7001);
    for (;;) {
        deviceConnected = probeDevices();
        ndt::NodeHeartbeat heartbeat;
        heartbeat.set_node_id(id); heartbeat.set_host(host); heartbeat.set_port(port);
        heartbeat.set_active_connections(activeConnections);
        heartbeat.set_running_tasks(runningTasks);
        heartbeat.set_device_connected(deviceConnected);
        heartbeat.set_available(true);
        heartbeat.set_heartbeat_timestamp_ms(ndtapp::epochMs());
        ndt::Ack reply;
        grpc::ClientContext context; ndtapp::deadline(context);
        auto result = stub->Heartbeat(&context, heartbeat, &reply);
        if (!result.ok()) std::cerr << "StatusServer heartbeat: " << result.error_message() << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
}

int main() {
    const int port = ndtapp::envInt("NDT_ACQ_PORT", 7001);
    auto status = ndt::StatusService::NewStub(grpc::CreateChannel(
        ndtapp::address("NDT_STATUS_HOST","NDT_STATUS_PORT",50052),
        grpc::InsecureChannelCredentials()));
    std::thread(heartbeatTask, status.get()).detach();
    std::thread(recoverReadyUploads).detach();
    boost::asio::io_context io;
    tcp::acceptor acceptor(io, {boost::asio::ip::make_address(ndtapp::env("NDT_ACQ_HOST","127.0.0.1")), uint16_t(port)});
    AsioIOServicePool pool(16);
    std::cout << "AcqServer listening on " << port << std::endl;
    for (;;) {
        tcp::socket socket(io); acceptor.accept(socket);
        if (activeConnections >= 16) { socket.close(); continue; }
        ++activeConnections;
        boost::asio::post(pool.GetIOService(),
            [connection = std::move(socket), statusPtr = status.get()]() mutable {
                clientTask(std::move(connection), statusPtr);
            });
    }
}
