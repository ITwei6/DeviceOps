#pragma once

#include <jsoncpp/json/json.h>

#include <atomic>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "device_gateway.pb.h"
#include "device_gateway/gateway_config.h"
#include "mqtt_client.h"

namespace brpc {
class Server;
}

namespace deviceops::gateway {

struct ForwardingCounter {
    uint64_t received = 0;
    uint64_t forwarded = 0;
    uint64_t dropped = 0;
    uint64_t failed = 0;
    int64_t updated_at = 0;
};

struct ConnectedDeviceRecord {
    std::string device_id;
    std::string client_id;
    std::string remote_addr;
    int64_t connected_at = 0;
    int64_t last_heartbeat_at = 0;
};

class GatewayServer {
public:
    explicit GatewayServer(GatewayConfig config);
    ~GatewayServer();

    bool start();
    void stop();
    bool running() const;
    GatewayStatus currentStatus() const;
    std::vector<ConnectedDevice> connectedDevices(const std::string& keyword, int page, int page_size, int64_t* total) const;
    std::vector<ForwardingStat> forwardingStats(const std::string& message_type) const;
    const std::string& gatewayId() const;

private:
    struct ParsedMqttMessage {
        std::string message_type;
        std::string device_id;
        std::string message_id;
        int64_t timestamp = 0;
        Json::Value payload;
    };

    void handleMqttMessage(const tewmqtt::MqttMessage& message);
    bool parsePayload(const std::string& topic, const std::string& payload, ParsedMqttMessage* parsed);
    void markReceived(const std::string& message_type);
    void markForwarded(const std::string& message_type);
    void markDropped(const std::string& message_type);
    void markFailed(const std::string& message_type);
    void updateDeviceView(const ParsedMqttMessage& parsed);

private:
    GatewayConfig _config;
    tewmqtt::MqttClient _mqtt_client;
    std::shared_ptr<brpc::Server> _rpc_server;
    std::atomic_bool _running{false};
    std::atomic<int64_t> _started_at{0};
    std::atomic<int64_t> _updated_at{0};
    mutable std::mutex _stats_mutex;
    std::map<std::string, ForwardingCounter> _stats;
    mutable std::mutex _devices_mutex;
    std::map<std::string, ConnectedDeviceRecord> _devices;
};

} // namespace deviceops::gateway
