#include "LogicSystem.h"

namespace ndtapp::file {
void LogicSystem::Query(const ndt::UploadInfo &request, ndt::UploadProgress &reply) {
    archive_.Query(request, reply);
}
void LogicSystem::Append(const ndt::UploadChunk &request, ndt::UploadProgress &reply) {
    archive_.Append(request, reply);
}
void LogicSystem::Complete(const ndt::UploadInfo &request, ndt::UploadProgress &reply) {
    archive_.Complete(request, reply);
}
}
