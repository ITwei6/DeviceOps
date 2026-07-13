#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#include "deviceops/db/odb_database.h"
#include "deviceops/mq/rabbitmq_event_publisher.h"
#include "diagnosis_service/diagnosis_rag_client.h"
#include "diagnosis_service/diagnosis_repository.h"
#include "diagnosis_service/diagnosis_service_impl.h"
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

    const int port = getenvIntOrDefault("DEVICEOPS_DIAGNOSIS_RPC_PORT", 9700);
    const auto db_config = deviceops::db::loadOdbConfigFromEnv();
    deviceops::db::ensureOdbSchema(db_config);

    deviceops::diagnosis_service::DiagnosisRepository repository(deviceops::db::createOdbDatabase(db_config));
    deviceops::diagnosis_service::DiagnosisRagClient rag_client(deviceops::diagnosis_service::loadDiagnosisRagConfigFromEnv());
    deviceops::mq::RabbitMqEventPublisher event_publisher(
        deviceops::mq::loadRabbitMqConfigFromEnv(),
        "diagnosis-service");
    auto server = tewrpc::RpcServerFactory::create(
        port,
        new deviceops::diagnosis_service::DiagnosisServiceImpl(&repository, &rag_client, &event_publisher));

    INF("diagnosis-service started: rpc_port={}", port);
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server->Stop(0);
    server->Join();
    return 0;
}
