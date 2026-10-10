#pragma once
#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ndtapp {
class GrpcChannelManager final {
public:
    static GrpcChannelManager &Instance();
    std::shared_ptr<grpc::Channel> Get(const std::string &address);
private:
    std::mutex mutex_;
    std::map<std::string, std::weak_ptr<grpc::Channel>> channels_;
};
}
