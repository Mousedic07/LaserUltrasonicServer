#include "NdtCommon.h"
#include "ConfigMgr.h"
#include "ndt.grpc.pb.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <unordered_map>
#include <windows.h>

namespace {
namespace fs = std::filesystem;
bool validId(const std::string &id) {
    static const std::regex pattern("^[A-Za-z0-9_-]{1,80}$");
    return std::regex_match(id, pattern);
}
std::string redisCommand(const std::vector<std::string> &args) {
    boost::asio::io_context io;
    ndtapp::tcp::socket socket(io);
    const std::string host = ndtapp::env("NDT_REDIS_HOST", "127.0.0.1");
    const int port = ndtapp::envInt("NDT_REDIS_PORT", 6379);
    socket.connect({boost::asio::ip::make_address(host), uint16_t(port)});
    auto send = [&socket](const std::vector<std::string> &parts) {
        std::string command = "*" + std::to_string(parts.size()) + "\r\n";
        for (const auto &part : parts)
            command += "$" + std::to_string(part.size()) + "\r\n" + part + "\r\n";
        boost::asio::write(socket, boost::asio::buffer(command));
        std::string line;
        for (;;) {
            char c; boost::asio::read(socket, boost::asio::buffer(&c, 1));
            if (c == '\n') break;
            if (c != '\r') line.push_back(c);
            if (line.size() > 1024) throw std::runtime_error("Redis reply too long");
        }
        if (line.empty() || line[0] == '-') throw std::runtime_error("Redis error: " + line);
        return line;
    };
    const std::string password = ndtapp::env("NDT_REDIS_PASSWORD", "");
    if (!password.empty()) send({"AUTH", password});
    return send(args);
}
void redisProgress(const ndt::UploadInfo &info, uint64_t confirmed, double speed,
                   const std::string &status) {
    const std::string key = "ndt:upload:" + info.upload_id();
    redisCommand({"HSET", key, "upload_id", info.upload_id(), "total_bytes",
       std::to_string(info.total_bytes()), "confirmed_bytes", std::to_string(confirmed),
       "speed_bytes_per_sec", std::to_string(speed), "status", status,
       "last_update_time", std::to_string(ndtapp::epochMs())});
    if (status == "complete") redisCommand({"EXPIRE", key, "86400"});
}
class FileLogic final : public ndt::ArchiveService::Service {
public:
    FileLogic() : root_(ndtapp::env("NDT_FILE_ROOT", "data")) { fs::create_directories(root_); }
    grpc::Status Query(grpc::ServerContext *, const ndt::UploadInfo *request,
                        ndt::UploadProgress *reply) override {
        try {
            if (!check(*request, reply)) return grpc::Status::OK;
            uint64_t size;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto meta = path(request->upload_id(), ".meta");
                const auto temp = path(request->upload_id(), ".part");
                const auto final = path(request->upload_id(), ".ndtbin");
                if (fs::exists(final)) size = request->total_bytes();
                else {
                    if (fs::exists(meta)) {
                        std::ifstream in(meta);
                        uint64_t stored = 0; in >> stored;
                        if (stored != request->total_bytes()) throw std::runtime_error("upload total changed");
                    } else {
                        std::ofstream out(meta, std::ios::trunc); out << request->total_bytes(); out.flush();
                    }
                    size = fs::exists(temp) ? fs::file_size(temp) : 0;
                }
                if (size > request->total_bytes()) throw std::runtime_error("stored file exceeds total");
            }
            fill(reply, *request, size, 0, size == request->total_bytes() ? "ready" : "uploading");
            redisProgress(*request, size, 0, reply->status());
            return grpc::Status::OK;
        } catch (const std::exception &e) { return grpc::Status(grpc::StatusCode::UNAVAILABLE, e.what()); }
    }
    grpc::Status Append(grpc::ServerContext *, const ndt::UploadChunk *request,
                         ndt::UploadProgress *reply) override {
        try {
            const auto &info = request->info();
            if (!check(info, reply)) return grpc::Status::OK;
            if (request->data().empty() || request->data().size() > 65536)
                return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "chunk must be 1..65536 bytes");
            const auto started = std::chrono::steady_clock::now();
            uint64_t size;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto meta = path(info.upload_id(), ".meta");
                const auto temp = path(info.upload_id(), ".part");
                if (!fs::exists(meta)) throw std::runtime_error("Query upload before Append");
                std::ifstream in(meta); uint64_t total = 0; in >> total;
                if (total != info.total_bytes()) throw std::runtime_error("upload total changed");
                size = fs::exists(temp) ? fs::file_size(temp) : 0;
                if (request->offset() != size) {
                    fill(reply, info, size, 0, "offset_mismatch");
                    reply->set_ok(false); return grpc::Status::OK;
                }
                if (size + request->data().size() > total) throw std::runtime_error("chunk exceeds total");
                std::ofstream out(temp, std::ios::binary | std::ios::app);
                out.write(request->data().data(), std::streamsize(request->data().size())); out.flush();
                if (!out) throw std::runtime_error("disk write failed");
                HANDLE file = CreateFileW(temp.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE || !FlushFileBuffers(file)) {
                    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
                    throw std::runtime_error("disk flush failed");
                }
                CloseHandle(file);
                size += request->data().size();
            }
            const double elapsed = std::max(0.001, std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count());
            const double speed = request->data().size() / elapsed;
            fill(reply, info, size, speed, "uploading");
            redisProgress(info, size, speed, "uploading");
            return grpc::Status::OK;
        } catch (const std::exception &e) { return grpc::Status(grpc::StatusCode::UNAVAILABLE, e.what()); }
    }
    grpc::Status Complete(grpc::ServerContext *, const ndt::UploadInfo *request,
                           ndt::UploadProgress *reply) override {
        try {
            if (!check(*request, reply)) return grpc::Status::OK;
            if (request->sha256().size() != 64) throw std::runtime_error("SHA-256 required");
            const auto temp = path(request->upload_id(), ".part");
            const auto final = path(request->upload_id(), ".ndtbin");
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!fs::exists(final)) {
                    if (!fs::exists(temp) || fs::file_size(temp) != request->total_bytes())
                        throw std::runtime_error("incomplete file");
                    if (ndtapp::sha256File(temp.string()) != request->sha256())
                        throw std::runtime_error("SHA-256 mismatch");
                    fs::rename(temp, final);
                    fs::remove(path(request->upload_id(), ".meta"));
                } else if (fs::file_size(final) != request->total_bytes() ||
                           ndtapp::sha256File(final.string()) != request->sha256())
                    throw std::runtime_error("existing archive differs");
            }
            fill(reply, *request, request->total_bytes(), 0, "complete");
            reply->set_filename(final.filename().string());
            redisProgress(*request, request->total_bytes(), 0, "complete");
            return grpc::Status::OK;
        } catch (const std::exception &e) { return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what()); }
    }
private:
    bool check(const ndt::UploadInfo &info, ndt::UploadProgress *reply) {
        if (!validId(info.upload_id()) || info.total_bytes() == 0 || info.total_bytes() > uint64_t(4) * 1024 * 1024 * 1024) {
            reply->set_error("invalid upload_id or total_bytes"); return false;
        }
        return true;
    }
    fs::path path(const std::string &id, const std::string &extension) const { return root_ / (id + extension); }
    static void fill(ndt::UploadProgress *reply, const ndt::UploadInfo &info, uint64_t size,
                     double speed, const std::string &status) {
        reply->set_ok(true); reply->set_confirmed_bytes(size); reply->set_total_bytes(info.total_bytes());
        reply->set_speed_bytes_per_sec(speed); reply->set_status(status);
        reply->set_filename(info.upload_id() + ".ndtbin");
    }
    fs::path root_;
    std::mutex mutex_;
};
}
int main(int argc, char **argv) {
    ndtapp::ConfigMgr::Instance().Load("FileServer", argc, argv);
    const std::string listen = ndtapp::address("NDT_FILE_HOST", "NDT_FILE_PORT", 50054);
    FileLogic service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server) { std::cerr << "FileServer bind failed: " << listen << '\n'; return 1; }
    std::cout << "FileServer listening on " << listen << std::endl;
    server->Wait();
}
