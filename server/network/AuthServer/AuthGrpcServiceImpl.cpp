#include "AuthGrpcServiceImpl.h"
#include <cppconn/exception.h>
#include <iostream>

namespace ndtapp::auth {
grpc::Status AuthGrpcServiceImpl::Verify(grpc::ServerContext *, const ndt::LoginRequest *request,
                                         ndt::LoginReply *reply) {
    try {
        const auto result = logic_.Verify(request->username(), request->password());
        reply->set_ok(result.ok); reply->set_user_id(result.userId);
        reply->set_role(result.role); reply->set_error(result.error);
        return grpc::Status::OK;
    } catch (const sql::SQLException &e) {
        std::cerr << "MySQL error: " << e.what() << '\n';
        return {grpc::StatusCode::UNAVAILABLE, "database unavailable"};
    } catch (const std::runtime_error &e) {
        std::cerr << "Auth dependency error: " << e.what() << '\n';
        return {grpc::StatusCode::UNAVAILABLE, "database unavailable"};
    } catch (const std::exception &e) {
        std::cerr << "Auth error: " << e.what() << '\n';
        return {grpc::StatusCode::INTERNAL, "authentication failed"};
    }
}
}
