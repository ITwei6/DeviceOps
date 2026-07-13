#pragma once

#include <memory>
#include <string>

#include "device_gateway/gateway_config.h"
#include "device_gateway/parsed_mqtt_message.h"

namespace brpc {
class Channel;
}

namespace deviceops::event {
class EventService_Stub;
}

namespace deviceops::log {
class LogService_Stub;
}

namespace deviceops::telemetry {
class TelemetryService_Stub;
}

namespace deviceops::gateway {

class DownstreamForwarder {
public:
    explicit DownstreamForwarder(DownstreamRpcConfig config);
    ~DownstreamForwarder();

    bool forward(const ParsedMqttMessage& message, std::string* error);

private:
    bool forwardTelemetry(const ParsedMqttMessage& message, std::string* error);
    bool forwardAlarm(const ParsedMqttMessage& message, std::string* error);
    bool forwardLog(const ParsedMqttMessage& message, std::string* error);

private:
    DownstreamRpcConfig _config;
    std::unique_ptr<brpc::Channel> _telemetry_channel;
    std::unique_ptr<brpc::Channel> _event_channel;
    std::unique_ptr<brpc::Channel> _log_channel;
    std::unique_ptr<deviceops::telemetry::TelemetryService_Stub> _telemetry_stub;
    std::unique_ptr<deviceops::event::EventService_Stub> _event_stub;
    std::unique_ptr<deviceops::log::LogService_Stub> _log_stub;
};

} // namespace deviceops::gateway
