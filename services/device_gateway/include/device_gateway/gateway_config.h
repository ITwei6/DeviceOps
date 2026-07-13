#pragma once

#include <string>
#include <vector>

#include "mqtt_client.h"

namespace deviceops::gateway {

struct DownstreamRpcConfig {
    bool enabled = true;
    std::string telemetry_addr = "127.0.0.1:9301";
    std::string event_addr = "127.0.0.1:9401";
    std::string log_addr = "127.0.0.1:9501";
    int timeout_ms = 3000;
};

struct GatewayConfig {
    std::string gateway_id = "device-gateway-001";
    int rpc_port = 9101;
    tewmqtt::MqttConfig mqtt;
    DownstreamRpcConfig downstream;
    std::vector<std::string> subscribe_topics;
};

GatewayConfig loadGatewayConfigFromEnv();

} // namespace deviceops::gateway
