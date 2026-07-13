#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <jsoncpp/json/json.h>

#include "mq.h"

namespace deviceops::mq {

struct RabbitMqConfig {
    bool enabled = false;
    std::string url = "amqp://admin:123456@rabbitmq-service:5672/";
};

RabbitMqConfig loadRabbitMqConfigFromEnv();

class RabbitMqEventPublisher {
public:
    struct Route {
        const char* exchange;
        const char* queue;
        const char* routing_key;
    };

    RabbitMqEventPublisher(RabbitMqConfig config, std::string source);
    ~RabbitMqEventPublisher();

    RabbitMqEventPublisher(const RabbitMqEventPublisher&) = delete;
    RabbitMqEventPublisher& operator=(const RabbitMqEventPublisher&) = delete;

    bool enabled() const;

    bool publishTelemetryStatus(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);
    bool publishTelemetryOffline(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);
    bool publishAlarmCreated(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);
    bool publishLogReceived(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);
    bool publishKnowledgeIndexRequested(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);
    bool publishDiagnosisTaskCreated(const Json::Value& payload, const std::string& trace_id, std::string* error = nullptr);

private:
    void declareTopology();
    bool publish(const Route& route,
        const std::string& message_type,
        const Json::Value& payload,
        const std::string& trace_id,
        std::string* error);

    RabbitMqConfig _config;
    std::string _source;
    std::shared_ptr<tewmq::MQClient> _client;
    std::mutex _mutex;
};

} // namespace deviceops::mq
