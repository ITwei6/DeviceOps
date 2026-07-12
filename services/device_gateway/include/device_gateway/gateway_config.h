#pragma once

#include <string>
#include <vector>

#include "mqtt_client.h"

namespace deviceops::gateway {

struct GatewayConfig {
    std::string gateway_id = "device-gateway-001";
    int rpc_port = 9101;
    tewmqtt::MqttConfig mqtt;
    std::vector<std::string> subscribe_topics;
};

GatewayConfig loadGatewayConfigFromEnv();

} // namespace deviceops::gateway
