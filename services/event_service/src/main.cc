#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#include "deviceops/db/odb_database.h"
#include "deviceops/mq/rabbitmq_event_publisher.h"
#include "event_service/event_repository.h"
#include "event_service/event_service_impl.h"
#include "log.h"
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

    const int port = getenvIntOrDefault("DEVICEOPS_EVENT_RPC_PORT", 9401);
    const auto db_config = deviceops::db::loadOdbConfigFromEnv();
    deviceops::db::ensureOdbSchema(db_config);
    deviceops::event_service::EventRepository repository(deviceops::db::createOdbDatabase(db_config));
    deviceops::mq::RabbitMqEventPublisher event_publisher(
        deviceops::mq::loadRabbitMqConfigFromEnv(),
        "event-service");
    auto server = tewrpc::RpcServerFactory::create(
        port,
        new deviceops::event_service::EventServiceImpl(&repository, &event_publisher));

    INF("event-service started: rpc_port={}", port);
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server->Stop(0);
    server->Join();
    return 0;
}
