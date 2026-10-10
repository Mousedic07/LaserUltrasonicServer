#pragma once
#include "ArchiveLogic.h"

namespace ndtapp::file {
class LogicSystem final {
public:
    LogicSystem(FileManager &files, RedisMgr &redis) : archive_(files, redis) {}
    void Query(const ndt::UploadInfo &request, ndt::UploadProgress &reply);
    void Append(const ndt::UploadChunk &request, ndt::UploadProgress &reply);
    void Complete(const ndt::UploadInfo &request, ndt::UploadProgress &reply);
private:
    ArchiveLogic archive_;
};
}
