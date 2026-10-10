#include "MysqlDao.h"
#include <windows.h>
#include <bcrypt.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <stdexcept>
#include <vector>

namespace ndtapp::auth {
namespace {
std::string pbkdf2Hex(const std::string &password, const std::string &saltHex) {
    if (saltHex.size() != 32) throw std::runtime_error("invalid password salt");
    std::vector<unsigned char> salt(16), output(32);
    for (std::size_t i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<unsigned char>(std::stoi(saltHex.substr(i * 2, 2), nullptr, 16));
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        throw std::runtime_error("PBKDF2 provider failed");
    const NTSTATUS status = BCryptDeriveKeyPBKDF2(alg,
        reinterpret_cast<PUCHAR>(const_cast<char *>(password.data())), ULONG(password.size()),
        salt.data(), ULONG(salt.size()), 200000, output.data(), ULONG(output.size()), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status != 0) throw std::runtime_error("PBKDF2 failed");
    static const char *hex = "0123456789abcdef";
    std::string result;
    for (unsigned char b : output) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
bool constantEqual(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
}
LoginResult MysqlDao::Verify(const std::string &user, const std::string &password) {
    auto connection = pool_.Acquire();
    std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(
        "SELECT id, role, salt_hex, password_hash_hex FROM users WHERE username = ? AND enabled = 1"));
    statement->setString(1, user);
    std::unique_ptr<sql::ResultSet> results(statement->executeQuery());
    if (!results->next()) return {false, 0, {}, "user not found"};
    const std::string expected = results->getString("password_hash_hex");
    if (!constantEqual(pbkdf2Hex(password, results->getString("salt_hex")), expected))
        return {false, 0, {}, "wrong password"};
    return {true, results->getUInt64("id"), results->getString("role"), {}};
}
}
