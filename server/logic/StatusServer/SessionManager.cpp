#include "SessionManager.h"
#include "NdtCommon.h"

namespace ndtapp::status {
std::string SessionManager::Issue(uint64_t userId) {
    const auto token = randomHex(24);
    const auto now = epochMs();
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = {userId, now + 5 * 60 * 1000};
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expires < now) it = sessions_.erase(it); else ++it;
    }
    return token;
}
uint64_t SessionManager::Validate(const std::string &token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = sessions_.find(token);
    return found != sessions_.end() && found->second.expires >= epochMs()
        ? found->second.userId : 0;
}
}
