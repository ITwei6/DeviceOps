#include <atomic>
#include <csignal>

#include "deviceops/simulator/robot_device_simulator.h"
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

    auto config = deviceops::simulator::loadSimulatorConfigFromEnv();
    deviceops::simulator::RobotDeviceSimulator simulator(config);

    if (!simulator.start()) {
        return 2;
    }

    simulator.run(&g_stop);
    return g_stop.load() ? 130 : 0;
}
