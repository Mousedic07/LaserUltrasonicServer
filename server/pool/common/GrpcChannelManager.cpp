#include "GrpcChannelManager.h"

namespace ndtapp {
GrpcChannelManager &GrpcChannelManager::Instance() {
    static GrpcChannelManager manager;
    return manager;
}
std::shared_ptr<grpc::Channel> GrpcChannelManager::Get(const std::string &address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto channel = channels_[address].lock();
    if (!channel) {
        channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        channels_[address] = channel;
    }
    return channel;
}
}
