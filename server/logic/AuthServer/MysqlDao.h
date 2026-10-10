#pragma once
#include "MySqlPool.h"
#include <cstdint>
#include <string>

namespace ndtapp::auth {
struct LoginResult {
    bool ok = false;
    uint64_t userId = 0;
    std::string role, error;
};
class MysqlDao final {
public:
    explicit MysqlDao(MySqlPool &pool) : pool_(pool) {}
    LoginResult Verify(const std::string &user, const std::string &password);
private:
    MySqlPool &pool_;
};
}
