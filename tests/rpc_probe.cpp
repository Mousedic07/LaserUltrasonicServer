#include "NdtCommon.h"
#include "ndt.grpc.pb.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace ndtapp;
namespace {
void require(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}
void statusTest() {
    auto stub = ndt::StatusService::NewStub(grpc::CreateChannel(
        address("NDT_STATUS_HOST","NDT_STATUS_PORT",50052), grpc::InsecureChannelCredentials()));
    auto beat = [&](const std::string &id, int port, int connections, int tasks, bool device) {
        ndt::NodeHeartbeat request;
        request.set_node_id(id); request.set_host("127.0.0.1"); request.set_port(port);
        request.set_active_connections(connections); request.set_running_tasks(tasks);
        request.set_device_connected(device); request.set_available(true);
        request.set_heartbeat_timestamp_ms(epochMs());
        ndt::Ack reply; grpc::ClientContext context; deadline(context);
        require(stub->Heartbeat(&context, request, &reply).ok() && reply.ok(), "heartbeat failed");
    };
    auto route = [&]() {
        ndt::RouteRequest request; request.set_user_id(1);
        ndt::RouteReply reply; grpc::ClientContext context; deadline(context);
        require(stub->Route(&context, request, &reply).ok() && reply.ok(), "route failed");
        return reply;
    };
    beat("test-a", 7991, 3, 1, true);
    beat("test-b", 7992, 1, 0, true);
    require(route().node_id() == "test-b", "low-load route wrong");
    beat("test-a", 7991, 1, 0, true);
    beat("test-b", 7992, 1, 0, true);
    const auto first = route();
    const auto second = route();
    require(first.node_id() != second.node_id(), "tie rotation failed");
    ndt::SessionRequest validate; validate.set_token(second.token());
    ndt::SessionReply validated; grpc::ClientContext context; deadline(context);
    require(stub->Validate(&context, validate, &validated).ok() && validated.ok(), "token validation failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    beat("test-a", 7991, 0, 0, true);
    require(route().node_id() == "test-a", "stale node was routed");
    beat("test-a", 7991, 0, 0, false);
    ndt::RouteRequest request; request.set_user_id(1);
    ndt::RouteReply reply; grpc::ClientContext failContext; deadline(failContext);
    require(stub->Route(&failContext, request, &reply).ok() && !reply.ok(), "unavailable device routed");
    std::cout << "status route/load/timeout/token passed\n";
}
void procTest() {
    auto stub = ndt::ProcessService::NewStub(grpc::CreateChannel(
        address("NDT_PROC_HOST","NDT_PROC_PORT",50053), grpc::InsecureChannelCredentials()));
    ndt::Waveform wave;
    wave.set_sample_rate_hz(20e6); wave.set_channel_id(2);
    for (int i = 0; i < 4096; ++i)
        wave.add_samples(float(std::sin(2 * 3.14159265358979323846 * 2.75e6 * i / 20e6)));
    ndt::Spectrum result; grpc::ClientContext context; deadline(context, 5000);
    require(stub->Fft(&context, wave, &result).ok(), "FFT RPC failed");
    require(std::abs(result.dominant_frequency_hz() - 2.75e6) < 20e6 / 4096, "FFT RPC peak wrong");
    require(result.frequency_bins_hz_size() == 2049, "FFT RPC bin count wrong");
    std::cout << "proc FFT RPC passed\n";
}
void fileTest() {
    const std::string data(190000, 'x');
    ndt::UploadInfo info;
    info.set_upload_id("test-" + randomHex(8));
    info.set_total_bytes(data.size()); info.set_sha256(sha256Hex(data));
    auto connect = [] { return ndt::ArchiveService::NewStub(grpc::CreateChannel(
        address("NDT_FILE_HOST","NDT_FILE_PORT",50054), grpc::InsecureChannelCredentials())); };
    auto stub = connect();
    auto query = [&](ndt::ArchiveService::Stub *client) {
        ndt::UploadProgress progress; grpc::ClientContext context; deadline(context);
        require(client->Query(&context, info, &progress).ok() && progress.ok(), "upload Query failed");
        return progress.confirmed_bytes();
    };
    require(query(stub.get()) == 0, "new upload nonempty");
    auto append = [&](ndt::ArchiveService::Stub *client, uint64_t offset, uint64_t size) {
        ndt::UploadChunk chunk; *chunk.mutable_info() = info;
        chunk.set_offset(offset); chunk.set_data(data.data() + offset, size);
        ndt::UploadProgress progress; grpc::ClientContext context; deadline(context);
        require(client->Append(&context, chunk, &progress).ok() && progress.ok(), "upload Append failed");
        require(progress.confirmed_bytes() == offset + size, "upload ACK wrong");
    };
    uint64_t offset = 0;
    while (offset < 100000) { auto size = std::min<uint64_t>(4096, 100000 - offset); append(stub.get(), offset, size); offset += size; }
    stub.reset(); // Simulated client disconnect before completion.
    stub = connect();
    require(query(stub.get()) == offset, "resume offset not persisted");
    while (offset < data.size()) { auto size = std::min<uint64_t>(4096, data.size() - offset); append(stub.get(), offset, size); offset += size; }
    ndt::UploadProgress done; grpc::ClientContext context; deadline(context, 30000);
    require(stub->Complete(&context, info, &done).ok() && done.ok(), "upload Complete failed");
    const std::string root = env("NDT_FILE_ROOT", "data");
    require(sha256File(root + "/" + done.filename()) == info.sha256(), "saved SHA-256 differs");
    std::cout << "file chunk/resume/SHA-256 passed\n";
}
void seedPartialUpload() {
    const std::string id = env("NDT_RECOVERY_ID", "");
    const std::string path = env("NDT_RECOVERY_SPOOL", "");
    require(!id.empty() && !path.empty(), "recovery seed environment missing");
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "recovery spool missing");
    const std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    ndt::UploadInfo info; info.set_upload_id(id); info.set_total_bytes(data.size());
    auto stub = ndt::ArchiveService::NewStub(grpc::CreateChannel(
        address("NDT_FILE_HOST","NDT_FILE_PORT",50054), grpc::InsecureChannelCredentials()));
    ndt::UploadProgress start; grpc::ClientContext queryContext; deadline(queryContext);
    require(stub->Query(&queryContext, info, &start).ok() && start.ok(), "seed Query failed");
    uint64_t offset = start.confirmed_bytes();
    const uint64_t half = data.size() / 2;
    while (offset < half) {
        const uint64_t size = std::min<uint64_t>(4096, half - offset);
        ndt::UploadChunk chunk; *chunk.mutable_info() = info;
        chunk.set_offset(offset); chunk.set_data(data.data() + offset, size);
        ndt::UploadProgress progress; grpc::ClientContext context; deadline(context);
        require(stub->Append(&context, chunk, &progress).ok() && progress.ok(), "seed Append failed");
        offset = progress.confirmed_bytes();
    }
    require(offset == half, "seed offset mismatch");
    std::cout << "partial upload seeded at " << offset << " bytes\n";
}
void authTest() {
    auto stub = ndt::AuthService::NewStub(grpc::CreateChannel(
        address("NDT_AUTH_HOST","NDT_AUTH_PORT",50051), grpc::InsecureChannelCredentials()));
    const std::string password = env("NDT_TEST_PASSWORD", "");
    require(!password.empty(), "NDT_TEST_PASSWORD required");
    auto verify = [&](const std::string &user, const std::string &pass) {
        ndt::LoginRequest request; request.set_username(user); request.set_password(pass);
        ndt::LoginReply reply; grpc::ClientContext context; deadline(context, 5000);
        require(stub->Verify(&context, request, &reply).ok(), "Auth RPC failed");
        return reply;
    };
    require(verify("demo", password).ok(), "correct login failed");
    require(!verify("demo", password + "wrong").ok(), "wrong password succeeded");
    require(!verify("missing-user", password).ok(), "unknown user succeeded");
    std::cout << "auth login/wrong password/unknown user passed\n";
}
void authDatabaseFailureTest() {
    auto stub = ndt::AuthService::NewStub(grpc::CreateChannel(
        address("NDT_AUTH_HOST","NDT_AUTH_PORT",50051), grpc::InsecureChannelCredentials()));
    ndt::LoginRequest request; request.set_username("demo"); request.set_password("unused");
    ndt::LoginReply reply; grpc::ClientContext context; deadline(context, 5000);
    auto result = stub->Verify(&context, request, &reply);
    require(!result.ok() && result.error_code() == grpc::StatusCode::UNAVAILABLE,
            "database failure was not surfaced as UNAVAILABLE");
    std::cout << "auth database failure passed\n";
}
void procFailureTest() {
    auto stub = ndt::ProcessService::NewStub(grpc::CreateChannel(
        address("NDT_PROC_HOST","NDT_PROC_PORT",50053), grpc::InsecureChannelCredentials()));
    ndt::Waveform request; request.set_sample_rate_hz(20e6);
    request.add_samples(1); request.add_samples(0);
    ndt::Spectrum reply; grpc::ClientContext context; deadline(context, 1500);
    const auto start = std::chrono::steady_clock::now();
    const auto result = stub->Fft(&context, request, &reply);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    require(!result.ok() && elapsed < 4.0, "unavailable ProcServer hung or falsely succeeded");
    std::cout << "ProcServer unavailability deadline passed\n";
}
}
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: rpc_probe status|proc|file|auth");
        const std::string mode = argv[1];
        if (mode == "status") statusTest();
        else if (mode == "proc") procTest();
        else if (mode == "file") fileTest();
        else if (mode == "file_seed") seedPartialUpload();
        else if (mode == "auth") authTest();
        else if (mode == "auth_db_failure") authDatabaseFailureTest();
        else if (mode == "proc_failure") procFailureTest();
        else throw std::runtime_error("unknown mode");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "TEST FAILED: " << e.what() << '\n';
        return 1;
    }
}
