#pragma once
#include "ndt.pb.h"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace ndtapp::file {
struct AppendResult { bool offsetMatch; uint64_t confirmed; };
class FileManager final {
public:
    FileManager();
    uint64_t Query(const ndt::UploadInfo &info);
    AppendResult Append(const ndt::UploadInfo &info, uint64_t offset,
                        const std::string &bytes);
    std::string Complete(const ndt::UploadInfo &info);
private:
    std::filesystem::path Path(const std::string &id, const char *extension) const;
    std::filesystem::path root_;
    std::mutex mutex_;
};
}
