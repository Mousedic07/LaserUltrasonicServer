#include "GrpcServerRunner.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace ndtapp {
namespace {
std::atomic<bool> stopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free, "signal flag must be lock-free");
void requestStop(int) { stopRequested.store(true, std::memory_order_relaxed); }
}
void WaitForGrpcShutdown(grpc::Server &server) {
    stopRequested.store(false, std::memory_order_relaxed);
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    std::atomic<bool> done{false};
    std::thread watcher([&] {
        while (!done.load() && !stopRequested.load(std::memory_order_relaxed))
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (stopRequested.load(std::memory_order_relaxed)) server.Shutdown();
    });
    server.Wait();
    done = true;
    watcher.join();
}
}
