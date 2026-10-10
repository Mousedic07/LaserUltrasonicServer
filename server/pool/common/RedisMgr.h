#pragma once
#include "RedisConPool.h"
#include "ndt.pb.h"

namespace ndtapp {
class RedisMgr final {
public:
    explicit RedisMgr(RedisConPool &pool) : pool_(pool) {}
    void Progress(const ndt::UploadInfo &info, uint64_t confirmed, double speed,
                  const std::string &status);
private:
    RedisConPool &pool_;
};
}
