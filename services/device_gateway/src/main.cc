#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "device_gateway/gateway_config.h"
#include "device_gateway/gateway_server.h"
#include "log.h"

namespace {

std::atomic_bool g_stop{false};

void handleSignal(int) {
    g_stop.store(true);
}

} // namespace

int main() {
    tewlog::tewlog_init();
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    deviceops::gateway::GatewayServer server(deviceops::gateway::loadGatewayConfigFromEnv());
    if (!server.start()) {
        return 2;
    }

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
    return 0;
}
