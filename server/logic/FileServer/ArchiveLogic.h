#pragma once
#include "FileManager.h"
#include "RedisMgr.h"
#include "ndt.pb.h"

namespace ndtapp::file {
class ArchiveLogic final {
public:
    ArchiveLogic(FileManager &files, RedisMgr &redis) : files_(files), redis_(redis) {}
    void Query(const ndt::UploadInfo &request, ndt::UploadProgress &reply);
    void Append(const ndt::UploadChunk &request, ndt::UploadProgress &reply);
    void Complete(const ndt::UploadInfo &request, ndt::UploadProgress &reply);
private:
    static bool Check(const ndt::UploadInfo &info, ndt::UploadProgress &reply);
    static void Fill(ndt::UploadProgress &reply, const ndt::UploadInfo &info,
                     uint64_t confirmed, double speed, const std::string &status);
    FileManager &files_;
    RedisMgr &redis_;
};
}
