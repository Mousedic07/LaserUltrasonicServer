#pragma once
#include "NdtCommon.h"
#include "server/network/AcqServer/FileGrpcClient.h"
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

namespace ndtapp::acq {
class CSession;
struct FileChunk { uint64_t offset = 0; std::string bytes; };
class FileUploadTask final {
public:
    static void Run(std::shared_ptr<CSession> session, BoundedQueue<FileChunk> &queue,
                    FileGrpcClient &client, const std::string &scanId, uint64_t total,
                    const std::filesystem::path &spool, const std::atomic<bool> &scanComplete);
    static void Recover(FileGrpcClient &client);
};
}
