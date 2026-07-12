#include "device_gateway/gateway_server.h"

#include <jsoncpp/json/json.h>

#include <sstream>
#include <utility>
#include <vector>

#include "log.h"

namespace deviceops::gateway {
namespace {

std::vector<std::string> splitTopic(const std::string& topic) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : topic) {
        if (ch == '/') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

void incReceived(ForwardingCounter& counter) {
    ++counter.received;
}

void incAccepted(ForwardingCounter& counter) {
    ++counter.accepted;
}

void incDropped(ForwardingCounter& counter) {
    ++counter.dropped;
}

void incFailed(ForwardingCounter& counter) {
    ++counter.failed;
}

} // namespace

GatewayServer::GatewayServer(GatewayConfig config)
    : _config(std::move(config))
    , _mqtt_client(_config.mqtt) {
}

GatewayServer::~GatewayServer() {
    stop();
}

bool GatewayServer::start() {
    _mqtt_client.setMessageHandler([this](const tewmqtt::MqttMessage& message) {
        handleMqttMessage(message);
    });

    for (const auto& topic : _config.subscribe_topics) {
        _mqtt_client.subscribe(topic, 1, nullptr);
    }

    if (!_mqtt_client.connect()) {
        ERR("device-gateway failed to connect MQTT broker: {}", _config.mqtt.broker_uri());
        return false;
    }

    _running.store(true);
    INF("device-gateway started: gateway_id={}, broker={}", _config.gateway_id, _config.mqtt.broker_uri());
    return true;
}

void GatewayServer::stop() {
    _running.store(false);
    _mqtt_client.disconnect();
}

bool GatewayServer::running() const {
    return _running.load();
}

GatewayStatus GatewayServer::currentStatus() const {
    GatewayStatus status;
    status.set_gateway_id(_config.gateway_id);
    status.set_mqtt_broker(_config.mqtt.broker_uri());
    status.set_mqtt_connected(_mqtt_client.connected());
    for (const auto& topic : _config.subscribe_topics) {
        status.add_subscribed_topics(topic);
    }
    return status;
}

void GatewayServer::handleMqttMessage(const tewmqtt::MqttMessage& message) {
    std::string message_type = "unknown";
    increment(message_type, incReceived);

    if (!validatePayload(message.topic, message.payload, &message_type)) {
        increment(message_type, incDropped);
        WRN("device-gateway dropped invalid MQTT payload: topic={}", message.topic);
        return;
    }

    increment(message_type, incAccepted);
    INF("device-gateway accepted MQTT message: type={}, topic={}, bytes={}",
        message_type,
        message.topic,
        message.payload.size());
}

bool GatewayServer::validatePayload(const std::string& topic, const std::string& payload, std::string* message_type) {
    const auto topic_parts = splitTopic(topic);
    if (topic_parts.size() != 3 || topic_parts[0] != "device") {
        if (message_type != nullptr) {
            *message_type = "unknown";
        }
        return false;
    }

    const std::string& topic_device_id = topic_parts[1];
    const std::string& type = topic_parts[2];
    if (message_type != nullptr) {
        *message_type = type;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream input(payload);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        WRN("device-gateway JSON parse failed: {}", errors);
        return false;
    }

    if (!root.isMember("message_id") || !root["message_id"].isString()) {
        return false;
    }
    if (!root.isMember("device_id") || !root["device_id"].isString()) {
        return false;
    }
    if (!root.isMember("timestamp") || !root["timestamp"].isNumeric()) {
        return false;
    }
    if (root["device_id"].asString() != topic_device_id) {
        WRN("device-gateway topic device_id mismatch: topic={}, payload={}",
            topic_device_id,
            root["device_id"].asString());
        return false;
    }

    return type == "register" || type == "telemetry" || type == "alarm" || type == "log" || type == "heartbeat";
}

void GatewayServer::increment(const std::string& message_type, void (*field)(ForwardingCounter&)) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    field(_stats[message_type]);
}

} // namespace deviceops::gateway
