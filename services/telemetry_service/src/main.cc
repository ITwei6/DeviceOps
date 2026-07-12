#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#include "log.h"
#include "rpc.h"
#include "telemetry_service/telemetry_repository.h"
#include "telemetry_service/telemetry_service_impl.h"

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

    const int port = getenvIntOrDefault("DEVICEOPS_TELEMETRY_RPC_PORT", 9301);
    deviceops::telemetry_service::TelemetryRepository repository(deviceops::telemetry_service::loadRedisConfigFromEnv());
    auto server = tewrpc::RpcServerFactory::create(
        port,
        new deviceops::telemetry_service::TelemetryServiceImpl(&repository));

    INF("telemetry-service started: rpc_port={}, redis_enabled={}", port, repository.redisEnabled());
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server->Stop(0);
    server->Join();
    return 0;
}
