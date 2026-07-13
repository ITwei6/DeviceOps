#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>

#include "deviceops/db/odb_database.h"
#include "knowledge_service/knowledge_repository.h"
#include "knowledge_service/knowledge_service_impl.h"
#include "knowledge_service/rag_client.h"
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

    const int port = getenvIntOrDefault("DEVICEOPS_KNOWLEDGE_RPC_PORT", 9600);
    const auto db_config = deviceops::db::loadOdbConfigFromEnv();
    deviceops::db::ensureOdbSchema(db_config);

    deviceops::knowledge_service::KnowledgeRepository repository(deviceops::db::createOdbDatabase(db_config));
    deviceops::knowledge_service::RagClient rag_client(deviceops::knowledge_service::loadRagConfigFromEnv());
    auto server = tewrpc::RpcServerFactory::create(
        port,
        new deviceops::knowledge_service::KnowledgeServiceImpl(&repository, &rag_client));

    INF("knowledge-service started: rpc_port={}", port);
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server->Stop(0);
    server->Join();
    return 0;
}
