#include "deviceops/simulator/robot_device_simulator.h"

#include <jsoncpp/json/json.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>

#include "log.h"

namespace deviceops::simulator {
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

std::string writeJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

double roundTo(double value, double scale) {
    return std::round(value * scale) / scale;
}

} // namespace

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

SimulatorConfig loadSimulatorConfigFromEnv() {
    SimulatorConfig config;
    config.device_id = getenvOrDefault("DEVICEOPS_SIM_DEVICE_ID", config.device_id);
    config.device_name = getenvOrDefault("DEVICEOPS_SIM_DEVICE_NAME", config.device_name);
    config.location = getenvOrDefault("DEVICEOPS_SIM_LOCATION", config.location);
    config.firmware_version = getenvOrDefault("DEVICEOPS_SIM_FIRMWARE_VERSION", config.firmware_version);
    config.publish_interval_ms = getenvIntOrDefault("DEVICEOPS_SIM_INTERVAL_MS", config.publish_interval_ms);
    config.loop_count = getenvIntOrDefault("DEVICEOPS_SIM_LOOP_COUNT", config.loop_count);
    config.high_temperature_period = getenvIntOrDefault("DEVICEOPS_SIM_HIGH_TEMP_PERIOD", config.high_temperature_period);
    config.error_code_period = getenvIntOrDefault("DEVICEOPS_SIM_ERROR_PERIOD", config.error_code_period);
    config.offline_period = getenvIntOrDefault("DEVICEOPS_SIM_OFFLINE_PERIOD", config.offline_period);

    config.mqtt.host = getenvOrDefault("DEVICEOPS_MQTT_HOST", "127.0.0.1");
    config.mqtt.port = getenvIntOrDefault("DEVICEOPS_MQTT_PORT", 1883);
    config.mqtt.client_id = getenvOrDefault("DEVICEOPS_MQTT_CLIENT_ID", config.device_id + "-simulator");
    config.mqtt.username = getenvOrDefault("DEVICEOPS_MQTT_USERNAME", "");
    config.mqtt.password = getenvOrDefault("DEVICEOPS_MQTT_PASSWORD", "");
    config.mqtt.keep_alive_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_KEEPALIVE_SECONDS", 60);
    config.mqtt.connect_timeout_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_CONNECT_TIMEOUT_SECONDS", 5);
    config.mqtt.retry_interval_seconds = getenvIntOrDefault("DEVICEOPS_MQTT_RETRY_INTERVAL_SECONDS", 2);
    return config;
}

RobotDeviceSimulator::RobotDeviceSimulator(SimulatorConfig config)
    : _config(std::move(config))
    , _mqtt_client(_config.mqtt)
    , _random(std::random_device{}()) {
}

bool RobotDeviceSimulator::start() {
    if (!_mqtt_client.connect()) {
        ERR("robot simulator failed to connect MQTT broker: {}", _config.mqtt.broker_uri());
        return false;
    }
    _running = true;
    INF("robot simulator connected: device_id={}, broker={}", _config.device_id, _config.mqtt.broker_uri());
    return publishRegister();
}

void RobotDeviceSimulator::stop() {
    _running = false;
    _mqtt_client.disconnect();
}

void RobotDeviceSimulator::run(const std::atomic_bool* stop_requested) {
    int sequence = 1;
    while (_running
        && (stop_requested == nullptr || !stop_requested->load())
        && (_config.loop_count <= 0 || sequence <= _config.loop_count)) {
        const Snapshot snapshot = nextSnapshot(sequence);
        const int64_t now_ms = currentUnixMillis();

        publishTelemetry(snapshot, sequence, now_ms);
        publishHeartbeat(snapshot, sequence, now_ms);
        if (!snapshot.error_code.empty() || snapshot.temperature >= 80.0 || !snapshot.online) {
            publishAlarm(snapshot, sequence, now_ms);
        }
        publishLog(snapshot, sequence, now_ms);

        _last_snapshot = snapshot;
        ++sequence;
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(100, _config.publish_interval_ms)));
    }
    stop();
}

RobotDeviceSimulator::Snapshot RobotDeviceSimulator::nextSnapshot(int sequence) {
    std::uniform_real_distribution<double> small_noise(-1.5, 1.5);
    std::uniform_real_distribution<double> speed_noise(0.0, 1.8);
    std::uniform_real_distribution<double> cpu_noise(0.1, 0.85);

    Snapshot snapshot = _last_snapshot;
    snapshot.battery = std::max(5.0, snapshot.battery - 0.15);
    snapshot.temperature = 42.0 + small_noise(_random);
    snapshot.speed = speed_noise(_random);
    snapshot.motor_current = 2.8 + small_noise(_random);
    snapshot.cpu_usage = cpu_noise(_random);
    snapshot.online = true;
    snapshot.run_mode = "auto";
    snapshot.error_code.clear();

    if (_config.high_temperature_period > 0 && sequence % _config.high_temperature_period == 0) {
        snapshot.temperature = 88.0 + small_noise(_random);
        snapshot.speed = 0.8;
        snapshot.error_code = "TEMP_001";
    } else if (_config.error_code_period > 0 && sequence % _config.error_code_period == 0) {
        snapshot.temperature = 55.0 + small_noise(_random);
        snapshot.error_code = "MOTOR_002";
        snapshot.run_mode = "manual";
    }

    if (_config.offline_period > 0 && sequence % _config.offline_period == 0) {
        snapshot.online = false;
        snapshot.speed = 0.0;
        snapshot.run_mode = "offline";
        snapshot.error_code = "COMM_001";
    }

    snapshot.battery = roundTo(snapshot.battery, 10.0);
    snapshot.temperature = roundTo(snapshot.temperature, 10.0);
    snapshot.speed = roundTo(snapshot.speed, 10.0);
    snapshot.motor_current = roundTo(snapshot.motor_current, 10.0);
    snapshot.cpu_usage = roundTo(snapshot.cpu_usage, 100.0);
    return snapshot;
}

bool RobotDeviceSimulator::publishRegister() {
    Json::Value root;
    root["message_id"] = messageId("register", 0);
    root["device_id"] = _config.device_id;
    root["timestamp"] = Json::Int64(currentUnixMillis());
    root["device_name"] = _config.device_name;
    root["device_type"] = "robot";
    root["model"] = "RBT-A1";
    root["manufacturer"] = "DeviceOpsLab";
    root["firmware_version"] = _config.firmware_version;
    root["location"] = _config.location;
    root["capabilities"].append("telemetry");
    root["capabilities"].append("alarm");
    root["capabilities"].append("log");
    root["capabilities"].append("heartbeat");
    return publishJson(topic("register"), writeJson(root), 1);
}

bool RobotDeviceSimulator::publishTelemetry(const Snapshot& snapshot, int sequence, int64_t now_ms) {
    Json::Value root;
    root["message_id"] = messageId("telemetry", sequence);
    root["device_id"] = _config.device_id;
    root["timestamp"] = Json::Int64(now_ms);
    root["battery"] = snapshot.battery;
    root["temperature"] = snapshot.temperature;
    root["speed"] = snapshot.speed;
    root["run_mode"] = snapshot.run_mode;
    root["online"] = snapshot.online;
    root["error_code"] = snapshot.error_code;
    root["metrics"]["motor_current"] = snapshot.motor_current;
    root["metrics"]["cpu_usage"] = snapshot.cpu_usage;
    return publishJson(topic("telemetry"), writeJson(root), 1);
}

bool RobotDeviceSimulator::publishHeartbeat(const Snapshot& snapshot, int sequence, int64_t now_ms) {
    Json::Value root;
    root["message_id"] = messageId("heartbeat", sequence);
    root["device_id"] = _config.device_id;
    root["timestamp"] = Json::Int64(now_ms);
    root["online"] = snapshot.online;
    root["run_mode"] = snapshot.run_mode;
    return publishJson(topic("heartbeat"), writeJson(root), 1);
}

bool RobotDeviceSimulator::publishAlarm(const Snapshot& snapshot, int sequence, int64_t now_ms) {
    Json::Value root;
    root["message_id"] = messageId("alarm", sequence);
    root["device_id"] = _config.device_id;
    root["timestamp"] = Json::Int64(now_ms);
    root["error_code"] = snapshot.error_code;
    root["metrics"]["temperature"] = snapshot.temperature;
    root["metrics"]["battery"] = snapshot.battery;
    root["metrics"]["speed"] = snapshot.speed;

    if (!snapshot.online) {
        root["alarm_type"] = "offline";
        root["severity"] = "critical";
        root["title"] = "device offline";
        root["description"] = "device heartbeat reports offline state";
    } else if (snapshot.temperature >= 80.0) {
        root["alarm_type"] = "temperature_high";
        root["severity"] = "critical";
        root["title"] = "device temperature high";
        root["description"] = "main controller temperature exceeds threshold";
    } else {
        root["alarm_type"] = "error_code";
        root["severity"] = "warning";
        root["title"] = "device error code reported";
        root["description"] = "robot controller reported a non-empty error code";
    }

    return publishJson(topic("alarm"), writeJson(root), 1);
}

bool RobotDeviceSimulator::publishLog(const Snapshot& snapshot, int sequence, int64_t now_ms) {
    Json::Value root;
    root["message_id"] = messageId("log", sequence);
    root["device_id"] = _config.device_id;
    root["timestamp"] = Json::Int64(now_ms);
    root["level"] = snapshot.error_code.empty() ? "info" : "warn";
    root["message"] = snapshot.error_code.empty() ? "robot telemetry uploaded" : "robot abnormal status detected";
    root["error_code"] = snapshot.error_code;
    root["context"]["battery"] = snapshot.battery;
    root["context"]["temperature"] = snapshot.temperature;
    root["context"]["speed"] = snapshot.speed;
    root["context"]["run_mode"] = snapshot.run_mode;
    root["tags"].append("robot");
    root["tags"].append(snapshot.online ? "online" : "offline");
    return publishJson(topic("log"), writeJson(root), 1);
}

bool RobotDeviceSimulator::publishJson(const std::string& topic_name, const std::string& payload, int qos) {
    const bool ok = _mqtt_client.publish(topic_name, payload, qos);
    if (ok) {
        INF("robot simulator published: topic={}, bytes={}", topic_name, payload.size());
    } else {
        ERR("robot simulator publish failed: topic={}", topic_name);
    }
    return ok;
}

std::string RobotDeviceSimulator::topic(const std::string& message_type) const {
    return "device/" + _config.device_id + "/" + message_type;
}

std::string RobotDeviceSimulator::messageId(const std::string& message_type, int sequence) const {
    std::ostringstream oss;
    oss << "msg-" << _config.device_id << "-" << message_type << "-" << std::setw(6) << std::setfill('0') << sequence;
    return oss.str();
}

} // namespace deviceops::simulator
