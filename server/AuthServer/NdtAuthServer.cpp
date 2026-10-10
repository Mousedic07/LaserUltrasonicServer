#include "NdtCommon.h"
#include "ndt.grpc.pb.h"
#include <bcrypt.h>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <stdexcept>

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
    for (std::size_t i = 0; i < a.size(); ++i) difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
class AuthLogic final : public ndt::AuthService::Service {
public:
    grpc::Status Verify(grpc::ServerContext *, const ndt::LoginRequest *request,
                         ndt::LoginReply *reply) override {
        if (request->username().empty() || request->password().empty()) {
            reply->set_error("missing credentials"); return grpc::Status::OK;
        }
        try {
            sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> connection(driver->connect(
                "tcp://" + ndtapp::address("NDT_MYSQL_HOST", "NDT_MYSQL_PORT", 3306),
                ndtapp::env("NDT_MYSQL_USER", "ndt"), ndtapp::env("NDT_MYSQL_PASSWORD", "")));
            connection->setSchema(ndtapp::env("NDT_MYSQL_DATABASE", "ndt"));
            std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(
                "SELECT id, role, salt_hex, password_hash_hex FROM users WHERE username = ? AND enabled = 1"));
            statement->setString(1, request->username());
            std::unique_ptr<sql::ResultSet> results(statement->executeQuery());
            if (!results->next()) { reply->set_error("user not found"); return grpc::Status::OK; }
            const std::string expected = results->getString("password_hash_hex");
            if (!constantEqual(pbkdf2Hex(request->password(), results->getString("salt_hex")), expected)) {
                reply->set_error("wrong password"); return grpc::Status::OK;
            }
            reply->set_ok(true);
            reply->set_user_id(results->getUInt64("id"));
            reply->set_role(results->getString("role"));
            return grpc::Status::OK;
        } catch (const sql::SQLException &e) {
            std::cerr << "MySQL error: " << e.what() << '\n';
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "database unavailable");
        } catch (const std::exception &e) {
            std::cerr << "Auth error: " << e.what() << '\n';
            return grpc::Status(grpc::StatusCode::INTERNAL, "authentication failed");
        }
    }
};
}
int main() {
    const std::string listen = ndtapp::address("NDT_AUTH_HOST", "NDT_AUTH_PORT", 50051);
    AuthLogic service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server) { std::cerr << "AuthServer bind failed: " << listen << '\n'; return 1; }
    std::cout << "AuthServer listening on " << listen << std::endl;
    server->Wait();
}
