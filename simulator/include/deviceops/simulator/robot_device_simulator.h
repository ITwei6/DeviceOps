#pragma once

#include <atomic>
#include <cstdint>
#include <random>
#include <string>

#include "mqtt_client.h"

namespace deviceops::simulator {

struct SimulatorConfig {
    std::string device_id = "robot-001";
    std::string device_name = "warehouse-robot-001";
    std::string location = "warehouse-a";
    std::string firmware_version = "1.0.0";
    int publish_interval_ms = 1000;
    int loop_count = 0;
    int high_temperature_period = 5;
    int error_code_period = 7;
    int offline_period = 11;
    tewmqtt::MqttConfig mqtt;
};

class RobotDeviceSimulator {
public:
    explicit RobotDeviceSimulator(SimulatorConfig config);

    bool start();
    void stop();
    void run(const std::atomic_bool* stop_requested = nullptr);

private:
    struct Snapshot {
        double battery = 100.0;
        double temperature = 42.0;
        double speed = 0.0;
        double motor_current = 2.8;
        double cpu_usage = 0.35;
        bool online = true;
        std::string run_mode = "auto";
        std::string error_code;
    };

    Snapshot nextSnapshot(int sequence);
    bool publishRegister();
    bool publishTelemetry(const Snapshot& snapshot, int sequence, int64_t now_ms);
    bool publishHeartbeat(const Snapshot& snapshot, int sequence, int64_t now_ms);
    bool publishAlarm(const Snapshot& snapshot, int sequence, int64_t now_ms);
    bool publishLog(const Snapshot& snapshot, int sequence, int64_t now_ms);
    bool publishJson(const std::string& topic, const std::string& payload, int qos);

    std::string topic(const std::string& message_type) const;
    std::string messageId(const std::string& message_type, int sequence) const;

private:
    SimulatorConfig _config;
    tewmqtt::MqttClient _mqtt_client;
    bool _running = false;
    std::mt19937 _random;
    Snapshot _last_snapshot;
};

SimulatorConfig loadSimulatorConfigFromEnv();
int64_t currentUnixMillis();

} // namespace deviceops::simulator
