#include "LogicSystem.h"

namespace ndtapp::auth {
LoginResult LogicSystem::Verify(const std::string &user, const std::string &password) {
    if (user.empty() || password.empty()) return {false, 0, {}, "missing credentials"};
    if (user.size() > 128 || password.size() > 1024) return {false, 0, {}, "invalid credentials"};
    return dao_.Verify(user, password);
}
}
