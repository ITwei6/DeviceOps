#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "device_gateway/gateway_config.h"
#include "mqtt_client.h"

namespace deviceops::gateway {

struct ForwardingCounter {
    uint64_t received = 0;
    uint64_t accepted = 0;
    uint64_t dropped = 0;
    uint64_t failed = 0;
};

class GatewayServer {
public:
    explicit GatewayServer(GatewayConfig config);
    ~GatewayServer();

    bool start();
    void stop();
    bool running() const;

private:
    void handleMqttMessage(const tewmqtt::MqttMessage& message);
    bool validatePayload(const std::string& topic, const std::string& payload, std::string* message_type);
    void increment(const std::string& message_type, void (*field)(ForwardingCounter&));

private:
    GatewayConfig _config;
    tewmqtt::MqttClient _mqtt_client;
    std::atomic_bool _running{false};
    std::mutex _stats_mutex;
    std::map<std::string, ForwardingCounter> _stats;
};

} // namespace deviceops::gateway
