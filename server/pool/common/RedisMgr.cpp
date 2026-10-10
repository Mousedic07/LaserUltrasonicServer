#include "RedisMgr.h"
#include "NdtCommon.h"

namespace ndtapp {
void RedisMgr::Progress(const ndt::UploadInfo &info, uint64_t confirmed,
                        double speed, const std::string &status) {
    const std::string key = "ndt:upload:" + info.upload_id();
    auto connection = pool_.Acquire();
    connection->Send({"HSET", key, "upload_id", info.upload_id(), "total_bytes",
        std::to_string(info.total_bytes()), "confirmed_bytes", std::to_string(confirmed),
        "speed_bytes_per_sec", std::to_string(speed), "status", status,
        "last_update_time", std::to_string(epochMs())});
    if (status == "complete") connection->Send({"EXPIRE", key, "86400"});
}
}
