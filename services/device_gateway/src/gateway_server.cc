#include "device_gateway/gateway_server.h"

#include <jsoncpp/json/json.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <utility>
#include <vector>

#include "device_gateway/gateway_rpc_service.h"
#include "log.h"
#include "rpc.h"

namespace deviceops::gateway {
namespace {

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

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

std::string messageTypeFromTopic(const std::string& topic) {
    const auto topic_parts = splitTopic(topic);
    if (topic_parts.size() != 3 || topic_parts[0] != "device") {
        return "unknown";
    }
    return topic_parts[2];
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
    const int64_t started_at = currentUnixMillis();
    _started_at.store(started_at);
    _updated_at.store(started_at);

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

    _rpc_server = tewrpc::RpcServerFactory::create(_config.rpc_port, new GatewayRpcService(this));
    _running.store(true);
    INF("device-gateway started: gateway_id={}, broker={}, rpc_port={}",
        _config.gateway_id,
        _config.mqtt.broker_uri(),
        _config.rpc_port);
    return true;
}

void GatewayServer::stop() {
    _running.store(false);
    if (_rpc_server) {
        _rpc_server->Stop(0);
        _rpc_server->Join();
        _rpc_server.reset();
    }
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
    status.set_started_at(_started_at.load());
    status.set_updated_at(_updated_at.load());
    for (const auto& topic : _config.subscribe_topics) {
        status.add_subscribed_topics(topic);
    }
    return status;
}

std::vector<ConnectedDevice> GatewayServer::connectedDevices(const std::string& keyword, int page, int page_size, int64_t* total) const {
    if (page <= 0) {
        page = 1;
    }
    if (page_size <= 0) {
        page_size = 20;
    }
    page_size = std::min(page_size, 100);

    std::vector<ConnectedDevice> all;
    {
        std::lock_guard<std::mutex> lock(_devices_mutex);
        for (const auto& item : _devices) {
            const auto& record = item.second;
            if (!keyword.empty()
                && record.device_id.find(keyword) == std::string::npos
                && record.client_id.find(keyword) == std::string::npos) {
                continue;
            }
            ConnectedDevice device;
            device.set_device_id(record.device_id);
            device.set_client_id(record.client_id);
            device.set_remote_addr(record.remote_addr);
            device.set_connected_at(record.connected_at);
            device.set_last_heartbeat_at(record.last_heartbeat_at);
            all.push_back(device);
        }
    }

    if (total != nullptr) {
        *total = static_cast<int64_t>(all.size());
    }

    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= all.size()) {
        return {};
    }
    const size_t end = std::min(all.size(), begin + static_cast<size_t>(page_size));
    return std::vector<ConnectedDevice>(all.begin() + begin, all.begin() + end);
}

std::vector<ForwardingStat> GatewayServer::forwardingStats(const std::string& message_type) const {
    std::vector<ForwardingStat> stats;
    std::lock_guard<std::mutex> lock(_stats_mutex);
    for (const auto& item : _stats) {
        if (!message_type.empty() && item.first != message_type) {
            continue;
        }
        const auto& counter = item.second;
        ForwardingStat stat;
        stat.set_message_type(item.first);
        stat.set_received_count(counter.received);
        stat.set_forwarded_count(counter.forwarded);
        stat.set_dropped_count(counter.dropped);
        stat.set_failed_count(counter.failed);
        stat.set_updated_at(counter.updated_at);
        stats.push_back(stat);
    }
    return stats;
}

const std::string& GatewayServer::gatewayId() const {
    return _config.gateway_id;
}

void GatewayServer::handleMqttMessage(const tewmqtt::MqttMessage& message) {
    const std::string message_type = messageTypeFromTopic(message.topic);
    markReceived(message_type);

    ParsedMqttMessage parsed;
    if (!parsePayload(message.topic, message.payload, &parsed)) {
        markDropped(message_type);
        WRN("device-gateway dropped invalid MQTT payload: topic={}", message.topic);
        return;
    }

    updateDeviceView(parsed);
    markForwarded(parsed.message_type);
    INF("device-gateway standardized MQTT message: type={}, device_id={}, topic={}, bytes={}",
        parsed.message_type,
        parsed.device_id,
        message.topic,
        message.payload.size());
}

bool GatewayServer::parsePayload(const std::string& topic, const std::string& payload, ParsedMqttMessage* parsed) {
    const auto topic_parts = splitTopic(topic);
    if (topic_parts.size() != 3 || topic_parts[0] != "device") {
        return false;
    }

    const std::string& topic_device_id = topic_parts[1];
    const std::string& type = topic_parts[2];

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream input(payload);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        WRN("device-gateway JSON parse failed: {}", errors);
        return false;
    }

    if (!root.isMember("message_id") || !root["message_id"].isString()) {
        WRN("device-gateway payload missing string message_id: topic={}", topic);
        return false;
    }
    if (!root.isMember("device_id") || !root["device_id"].isString()) {
        WRN("device-gateway payload missing string device_id: topic={}", topic);
        return false;
    }
    if (!root.isMember("timestamp") || !root["timestamp"].isNumeric()) {
        WRN("device-gateway payload missing numeric timestamp: topic={}", topic);
        return false;
    }
    if (root["device_id"].asString() != topic_device_id) {
        WRN("device-gateway topic device_id mismatch: topic={}, payload={}",
            topic_device_id,
            root["device_id"].asString());
        return false;
    }

    if (type != "register" && type != "telemetry" && type != "alarm" && type != "log" && type != "heartbeat") {
        WRN("device-gateway unsupported message type: {}", type);
        return false;
    }

    if (parsed != nullptr) {
        parsed->message_type = type;
        parsed->device_id = root["device_id"].asString();
        parsed->message_id = root["message_id"].asString();
        parsed->timestamp = root["timestamp"].asInt64();
        parsed->payload = root;
    }
    return true;
}

void GatewayServer::markReceived(const std::string& message_type) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    auto& counter = _stats[message_type];
    ++counter.received;
    counter.updated_at = currentUnixMillis();
    _updated_at.store(counter.updated_at);
}

void GatewayServer::markForwarded(const std::string& message_type) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    auto& counter = _stats[message_type];
    ++counter.forwarded;
    counter.updated_at = currentUnixMillis();
    _updated_at.store(counter.updated_at);
}

void GatewayServer::markDropped(const std::string& message_type) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    auto& counter = _stats[message_type];
    ++counter.dropped;
    counter.updated_at = currentUnixMillis();
    _updated_at.store(counter.updated_at);
}

void GatewayServer::markFailed(const std::string& message_type) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    auto& counter = _stats[message_type];
    ++counter.failed;
    counter.updated_at = currentUnixMillis();
    _updated_at.store(counter.updated_at);
}

void GatewayServer::updateDeviceView(const ParsedMqttMessage& parsed) {
    const int64_t now = currentUnixMillis();
    std::lock_guard<std::mutex> lock(_devices_mutex);
    auto& device = _devices[parsed.device_id];
    if (device.device_id.empty()) {
        device.device_id = parsed.device_id;
        device.client_id = parsed.device_id;
        device.remote_addr = _config.mqtt.broker_uri();
        device.connected_at = now;
    }
    if (parsed.message_type == "heartbeat") {
        device.last_heartbeat_at = parsed.timestamp > 0 ? parsed.timestamp : now;
    } else if (device.last_heartbeat_at == 0) {
        device.last_heartbeat_at = now;
    }
}

} // namespace deviceops::gateway
