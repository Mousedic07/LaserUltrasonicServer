#include "FileManager.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ndtapp::file {
namespace fs = std::filesystem;
FileManager::FileManager() : root_(ConfigMgr::Instance().Get(
    "Storage", "Root", "data", "NDT_FILE_ROOT")) { fs::create_directories(root_); }
fs::path FileManager::Path(const std::string &id, const char *extension) const {
    return root_ / (id + extension);
}
uint64_t FileManager::Query(const ndt::UploadInfo &info) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto meta = Path(info.upload_id(), ".meta");
    const auto temp = Path(info.upload_id(), ".part");
    const auto final = Path(info.upload_id(), ".ndtbin");
    if (fs::exists(final)) {
        if (fs::file_size(final) != info.total_bytes()) throw std::runtime_error("existing archive size changed");
        return info.total_bytes();
    }
    if (fs::exists(meta)) {
        std::ifstream in(meta); uint64_t stored = 0; in >> stored;
        if (!in || stored != info.total_bytes()) throw std::runtime_error("upload total changed");
    } else {
        std::ofstream out(meta, std::ios::trunc); out << info.total_bytes(); out.flush();
        if (!out) throw std::runtime_error("cannot persist upload metadata");
    }
    const auto size = fs::exists(temp) ? fs::file_size(temp) : 0;
    if (size > info.total_bytes()) throw std::runtime_error("stored file exceeds total");
    return size;
}
AppendResult FileManager::Append(const ndt::UploadInfo &info, uint64_t offset,
                                 const std::string &bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto meta = Path(info.upload_id(), ".meta");
    const auto temp = Path(info.upload_id(), ".part");
    if (!fs::exists(meta)) throw std::runtime_error("Query upload before Append");
    std::ifstream in(meta); uint64_t total = 0; in >> total;
    if (!in || total != info.total_bytes()) throw std::runtime_error("upload total changed");
    const auto size = fs::exists(temp) ? fs::file_size(temp) : 0;
    if (offset != size) return {false, size};
    if (size + bytes.size() > total) throw std::runtime_error("chunk exceeds total");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::app);
        out.write(bytes.data(), std::streamsize(bytes.size())); out.flush();
        if (!out) throw std::runtime_error("disk write failed");
    }
    HANDLE handle = CreateFileW(temp.wstring().c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        throw std::runtime_error("disk flush failed");
    }
    CloseHandle(handle);
    return {true, size + bytes.size()};
}
std::string FileManager::Complete(const ndt::UploadInfo &info) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto temp = Path(info.upload_id(), ".part");
    const auto final = Path(info.upload_id(), ".ndtbin");
    if (!fs::exists(final)) {
        if (!fs::exists(temp) || fs::file_size(temp) != info.total_bytes())
            throw std::runtime_error("incomplete file");
        if (sha256File(temp.string()) != info.sha256())
            throw std::runtime_error("SHA-256 mismatch");
        fs::rename(temp, final);
        fs::remove(Path(info.upload_id(), ".meta"));
    } else if (fs::file_size(final) != info.total_bytes() ||
               sha256File(final.string()) != info.sha256())
        throw std::runtime_error("existing archive differs");
    return final.filename().string();
}
}
