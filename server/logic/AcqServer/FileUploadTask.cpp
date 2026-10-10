#include "FileUploadTask.h"
#include "ConfigMgr.h"
#include "server/network/AcqServer/CSession.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <regex>
#include <thread>

namespace ndtapp::acq {
namespace fs = std::filesystem;
void FileUploadTask::Run(std::shared_ptr<CSession> session, BoundedQueue<FileChunk> &queue,
    FileGrpcClient &client, const std::string &scanId, uint64_t total,
    const fs::path &spool, const std::atomic<bool> &scanComplete) {
    ndt::UploadInfo info; info.set_upload_id(scanId); info.set_total_bytes(total);
    uint64_t confirmed = 0;
    auto query = [&]() -> bool {
        ndt::UploadProgress progress;
        const auto status = client.Query(info, progress);
        if (!status.ok() || !progress.ok()) return false;
        confirmed = progress.confirmed_bytes(); return true;
    };
    if (!query()) { session->Send(7, {{"status","FileServer unavailable"}}); return; }
    FileChunk chunk;
    while (queue.pop(chunk)) {
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (confirmed >= chunk.offset + chunk.bytes.size()) break;
            if (confirmed < chunk.offset) {
                if (!query() || confirmed < chunk.offset) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500)); continue;
                }
            }
            const std::size_t skip = std::size_t(confirmed - chunk.offset);
            ndt::UploadChunk request; *request.mutable_info() = info;
            request.set_offset(confirmed); request.set_data(chunk.bytes.data() + skip, chunk.bytes.size() - skip);
            ndt::UploadProgress progress;
            const auto status = client.Append(request, progress);
            if (status.ok() && progress.ok()) {
                confirmed = progress.confirmed_bytes();
                session->Send(7, {{"filename",progress.filename()},
                    {"percent",100.0 * double(confirmed) / double(total)},
                    {"speed_bytes_per_sec",progress.speed_bytes_per_sec()},
                    {"status",progress.status()}});
            } else {
                query(); std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        if (confirmed < chunk.offset + chunk.bytes.size()) {
            session->Send(7, {{"status","upload failed after retries"}}); return;
        }
    }
    if (!scanComplete || confirmed != total) {
        session->Send(7, {{"status","partial scan retained for recovery"}}); return;
    }
    info.set_sha256(sha256File(spool.string()));
    ndt::UploadProgress progress;
    const auto status = client.Complete(info, progress);
    if (!status.ok() || !progress.ok())
        session->Send(7, {{"status","archive validation failed"}});
    else {
        std::error_code ignored; fs::remove(spool.parent_path() / (scanId + ".ready"), ignored);
        session->Send(7, {{"filename",progress.filename()},{"percent",100.0},
                          {"speed_bytes_per_sec",0.0},{"status","complete"}});
    }
}
void FileUploadTask::Recover(FileGrpcClient &client) {
    try {
        const fs::path root = ConfigMgr::Instance().Get("Storage", "SpoolRoot", "spool", "NDT_SPOOL_ROOT");
        fs::create_directories(root);
        static const std::regex validId("^[0-9a-f]{24}$");
        for (const auto &entry : fs::directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".ready") continue;
            const std::string id = entry.path().stem().string();
            if (!std::regex_match(id, validId)) continue;
            const fs::path spool = root / (id + ".spool");
            try {
                uint64_t total = 0; std::string hash;
                {
                    std::ifstream marker(entry.path()); marker >> total >> hash;
                    if (!marker || hash.size() != 64 || !fs::exists(spool) || fs::file_size(spool) != total)
                        throw std::runtime_error("invalid ready marker or spool size");
                }
                if (sha256File(spool.string()) != hash)
                    throw std::runtime_error("spool SHA-256 changed");
                ndt::UploadInfo info; info.set_upload_id(id); info.set_total_bytes(total); info.set_sha256(hash);
                ndt::UploadProgress progress;
                const auto queried = client.Query(info, progress);
                if (!queried.ok() || !progress.ok()) throw std::runtime_error("FileServer Query failed");
                uint64_t offset = progress.confirmed_bytes();
                std::ifstream source(spool, std::ios::binary);
                std::array<char, 65536> data{};
                while (offset < total) {
                    source.seekg(std::streamoff(offset));
                    const auto length = std::size_t(std::min<uint64_t>(data.size(), total - offset));
                    source.read(data.data(), std::streamsize(length));
                    if (source.gcount() != std::streamsize(length)) throw std::runtime_error("spool read failed");
                    ndt::UploadChunk chunk; *chunk.mutable_info() = info;
                    chunk.set_offset(offset); chunk.set_data(data.data(), length);
                    ndt::UploadProgress appended;
                    const auto status = client.Append(chunk, appended);
                    if (!status.ok() || !appended.ok() || appended.confirmed_bytes() <= offset)
                        throw std::runtime_error("FileServer Append failed");
                    offset = appended.confirmed_bytes();
                }
                ndt::UploadProgress done;
                const auto finished = client.Complete(info, done);
                if (!finished.ok() || !done.ok()) throw std::runtime_error("FileServer Complete failed");
                fs::remove(entry.path());
                std::cout << "Recovered upload " << id << " from disk" << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "Upload recovery " << id << ": " << e.what() << '\n';
            }
        }
    } catch (const std::exception &e) { std::cerr << "Upload recovery setup: " << e.what() << '\n'; }
}
}
