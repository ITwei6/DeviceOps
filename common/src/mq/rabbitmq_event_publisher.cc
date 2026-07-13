#include "deviceops/mq/rabbitmq_event_publisher.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <sstream>

#include "log.h"

namespace deviceops::mq {
namespace {

constexpr RabbitMqEventPublisher::Route kTelemetryStatusRoute{
    "deviceops.telemetry.exchange",
    "telemetry.status.queue",
    "telemetry.status.updated"};
constexpr RabbitMqEventPublisher::Route kTelemetryOfflineRoute{
    "deviceops.telemetry.exchange",
    "telemetry.offline.queue",
    "telemetry.device.offline"};
constexpr RabbitMqEventPublisher::Route kAlarmCreatedRoute{
    "deviceops.event.exchange",
    "event.alarm.queue",
    "event.alarm.created"};
constexpr RabbitMqEventPublisher::Route kLogReceivedRoute{
    "deviceops.log.exchange",
    "log.ingest.queue",
    "log.device.received"};
constexpr RabbitMqEventPublisher::Route kDiagnosisTaskCreatedRoute{
    "deviceops.diagnosis.exchange",
    "diagnosis.task.queue",
    "diagnosis.task.created"};
constexpr RabbitMqEventPublisher::Route kKnowledgeIndexRequestedRoute{
    "deviceops.knowledge.exchange",
    "knowledge.index.queue",
    "knowledge.document.index_requested"};

std::vector<RabbitMqEventPublisher::Route> routesForSource(const std::string& source) {
    if (source == "telemetry-service") {
        return {kTelemetryStatusRoute, kTelemetryOfflineRoute};
    }
    if (source == "event-service") {
        return {kAlarmCreatedRoute};
    }
    if (source == "log-service") {
        return {kLogReceivedRoute};
    }
    if (source == "knowledge-service") {
        return {kKnowledgeIndexRequestedRoute};
    }
    if (source == "diagnosis-service") {
        return {kDiagnosisTaskCreatedRoute};
    }
    return {};
}

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string getenvOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

bool getenvBoolOrDefault(const char* name, bool fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "on";
}

std::string writeJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::string nextMessageId() {
    static std::atomic_uint64_t sequence{0};
    std::ostringstream out;
    out << "mq-" << currentUnixMillis() << "-" << sequence.fetch_add(1, std::memory_order_relaxed);
    return out.str();
}

} // namespace

RabbitMqConfig loadRabbitMqConfigFromEnv() {
    RabbitMqConfig config;
    config.enabled = getenvBoolOrDefault("DEVICEOPS_RABBITMQ_ENABLED", false);
    config.url = getenvOrDefault("DEVICEOPS_RABBITMQ_URL", config.url);
    return config;
}

RabbitMqEventPublisher::RabbitMqEventPublisher(RabbitMqConfig config, std::string source)
    : _config(std::move(config)),
      _source(std::move(source)) {
    if (!_config.enabled) {
        INF("rabbitmq publisher disabled: source={}", _source);
        return;
    }

    _client = tewmq::MQFactory::create<tewmq::MQClient>(_config.url);
    declareTopology();
    INF("rabbitmq publisher enabled: source={}, url={}", _source, _config.url);
}

RabbitMqEventPublisher::~RabbitMqEventPublisher() = default;

bool RabbitMqEventPublisher::enabled() const {
    return _config.enabled && _client != nullptr;
}

bool RabbitMqEventPublisher::publishTelemetryStatus(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kTelemetryStatusRoute, kTelemetryStatusRoute.routing_key, payload, trace_id, error);
}

bool RabbitMqEventPublisher::publishTelemetryOffline(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kTelemetryOfflineRoute, kTelemetryOfflineRoute.routing_key, payload, trace_id, error);
}

bool RabbitMqEventPublisher::publishAlarmCreated(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kAlarmCreatedRoute, kAlarmCreatedRoute.routing_key, payload, trace_id, error);
}

bool RabbitMqEventPublisher::publishLogReceived(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kLogReceivedRoute, kLogReceivedRoute.routing_key, payload, trace_id, error);
}

bool RabbitMqEventPublisher::publishKnowledgeIndexRequested(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kKnowledgeIndexRequestedRoute, kKnowledgeIndexRequestedRoute.routing_key, payload, trace_id, error);
}

bool RabbitMqEventPublisher::publishDiagnosisTaskCreated(const Json::Value& payload, const std::string& trace_id, std::string* error) {
    return publish(kDiagnosisTaskCreatedRoute, kDiagnosisTaskCreatedRoute.routing_key, payload, trace_id, error);
}

void RabbitMqEventPublisher::declareTopology() {
    for (const auto& route : routesForSource(_source)) {
        tewmq::declare_settings settings{
            .exchange = route.exchange,
            .exchange_type = tewmq::TOPIC,
            .queue = route.queue,
            .binding_key = route.routing_key};
        _client->declare(settings);
    }
}

bool RabbitMqEventPublisher::publish(const Route& route,
    const std::string& message_type,
    const Json::Value& payload,
    const std::string& trace_id,
    std::string* error) {
    if (!enabled()) {
        if (error != nullptr) {
            *error = "rabbitmq publisher disabled";
        }
        return false;
    }

    Json::Value envelope;
    envelope["message_id"] = nextMessageId();
    envelope["message_type"] = message_type;
    envelope["trace_id"] = trace_id;
    envelope["source"] = _source;
    envelope["timestamp"] = Json::Int64(currentUnixMillis());
    envelope["payload"] = payload;

    std::lock_guard<std::mutex> lock(_mutex);
    const bool ok = _client->publish(route.exchange, route.routing_key, writeJson(envelope));
    if (!ok && error != nullptr) {
        *error = "rabbitmq publish returned false";
    }
    return ok;
}

} // namespace deviceops::mq
