#pragma once
#include "MysqlDao.h"

namespace ndtapp::auth {
class LogicSystem final {
public:
    explicit LogicSystem(MySqlPool &pool) : dao_(pool) {}
    LoginResult Verify(const std::string &user, const std::string &password);
private:
    MysqlDao dao_;
};
}
