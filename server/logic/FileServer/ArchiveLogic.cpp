#include "ArchiveLogic.h"
#include <algorithm>
#include <chrono>
#include <regex>
#include <stdexcept>

namespace ndtapp::file {
bool ArchiveLogic::Check(const ndt::UploadInfo &info, ndt::UploadProgress &reply) {
    static const std::regex valid("^[A-Za-z0-9_-]{1,80}$");
    if (!std::regex_match(info.upload_id(), valid) || info.total_bytes() == 0 ||
        info.total_bytes() > uint64_t(4) * 1024 * 1024 * 1024) {
        reply.set_error("invalid upload_id or total_bytes"); return false;
    }
    return true;
}
void ArchiveLogic::Fill(ndt::UploadProgress &reply, const ndt::UploadInfo &info,
                         uint64_t confirmed, double speed, const std::string &status) {
    reply.set_ok(true); reply.set_confirmed_bytes(confirmed);
    reply.set_total_bytes(info.total_bytes()); reply.set_speed_bytes_per_sec(speed);
    reply.set_status(status); reply.set_filename(info.upload_id() + ".ndtbin");
}
void ArchiveLogic::Query(const ndt::UploadInfo &request, ndt::UploadProgress &reply) {
    if (!Check(request, reply)) return;
    const auto size = files_.Query(request);
    Fill(reply, request, size, 0, size == request.total_bytes() ? "ready" : "uploading");
    redis_.Progress(request, size, 0, reply.status());
}
void ArchiveLogic::Append(const ndt::UploadChunk &request, ndt::UploadProgress &reply) {
    const auto &info = request.info();
    if (!Check(info, reply)) return;
    if (request.data().empty() || request.data().size() > 65536)
        throw std::invalid_argument("chunk must be 1..65536 bytes");
    const auto started = std::chrono::steady_clock::now();
    const auto result = files_.Append(info, request.offset(), request.data());
    if (!result.offsetMatch) {
        Fill(reply, info, result.confirmed, 0, "offset_mismatch");
        reply.set_ok(false); return;
    }
    const double elapsed = std::max(0.001, std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count());
    const double speed = request.data().size() / elapsed;
    Fill(reply, info, result.confirmed, speed, "uploading");
    redis_.Progress(info, result.confirmed, speed, "uploading");
}
void ArchiveLogic::Complete(const ndt::UploadInfo &request, ndt::UploadProgress &reply) {
    if (!Check(request, reply)) return;
    if (request.sha256().size() != 64) throw std::invalid_argument("SHA-256 required");
    const auto filename = files_.Complete(request);
    Fill(reply, request, request.total_bytes(), 0, "complete");
    reply.set_filename(filename);
    redis_.Progress(request, request.total_bytes(), 0, "complete");
}
}
