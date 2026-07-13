#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#include "log.h"
#include "log_service/log_repository.h"
#include "log_service/log_service_impl.h"
#include "rpc.h"

namespace {

std::atomic_bool g_stop{false};

void handleSignal(int) {
    g_stop.store(true);
}

int getenvIntOrDefault(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

int main() {
    tewlog::tewlog_init();
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const int port = getenvIntOrDefault("DEVICEOPS_LOG_RPC_PORT", 9501);
    deviceops::log_service::LogRepository repository(deviceops::log_service::loadLogStoreConfigFromEnv());
    auto server = tewrpc::RpcServerFactory::create(
        port,
        new deviceops::log_service::LogServiceImpl(&repository));

    INF("log-service started: rpc_port={}", port);
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server->Stop(0);
    server->Join();
    return 0;
}
