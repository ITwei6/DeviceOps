#include "device_gateway/gateway_config.h"

#include <cstdlib>
#include <string>

namespace deviceops::gateway {
namespace {

std::string getenvOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
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

bool getenvBoolOrDefault(const char* name, bool fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    const std::string normalized(value);
    return normalized == "1" || normalized == "true" || normalized == "TRUE" || normalized == "yes" || normalized == "YES";
}

} // namespace

GatewayConfig loadGatewayConfigFromEnv() {
    GatewayConfig config;
    config.gateway_id = getenvOrDefault("DEVICEOPS_GATEWAY_ID", config.gateway_id);
    config.rpc_port = getenvIntOrDefault("DEVICEOPS_GATEWAY_RPC_PORT", config.rpc_port);
    config.mqtt.host = getenvOrDefault("DEVICEOPS_MQTT_HOST", "127.0.0.1");
    config.mqtt.port = getenvIntOrDefault("DEVICEOPS_MQTT_PORT", 1883);
    config.mqtt.client_id = getenvOrDefault("DEVICEOPS_MQTT_CLIENT_ID", config.gateway_id);
    config.mqtt.username = getenvOrDefault("DEVICEOPS_MQTT_USERNAME", "");
    config.mqtt.password = getenvOrDefault("DEVICEOPS_MQTT_PASSWORD", "");
    config.mqtt.keep_alive_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_KEEPALIVE_SECONDS", 60);
    config.mqtt.connect_timeout_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_CONNECT_TIMEOUT_SECONDS", 5);
    config.mqtt.retry_interval_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_RETRY_INTERVAL_SECONDS", 2);

    config.downstream.enabled = getenvBoolOrDefault("DEVICEOPS_DOWNSTREAM_RPC_ENABLED", config.downstream.enabled);
    config.downstream.device_addr = getenvOrDefault("DEVICEOPS_DEVICE_RPC_ADDR", config.downstream.device_addr);
    config.downstream.telemetry_addr = getenvOrDefault("DEVICEOPS_TELEMETRY_RPC_ADDR", config.downstream.telemetry_addr);
    config.downstream.event_addr = getenvOrDefault("DEVICEOPS_EVENT_RPC_ADDR", config.downstream.event_addr);
    config.downstream.log_addr = getenvOrDefault("DEVICEOPS_LOG_RPC_ADDR", config.downstream.log_addr);
    config.downstream.timeout_ms = getenvIntOrDefault("DEVICEOPS_DOWNSTREAM_RPC_TIMEOUT_MS", config.downstream.timeout_ms);

    config.subscribe_topics = {
        "device/+/register",
        "device/+/telemetry",
        "device/+/alarm",
        "device/+/log",
        "device/+/heartbeat",
    };
    return config;
}

} // namespace deviceops::gateway
