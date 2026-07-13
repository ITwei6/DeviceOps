#include "device_gateway/downstream_forwarder.h"

#include <brpc/channel.h>
#include <brpc/controller.h>
#include <jsoncpp/json/json.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include "common.pb.h"
#include "device.pb.h"
#include "event.pb.h"
#include "log.h"
#include "log.pb.h"
#include "telemetry.pb.h"

namespace deviceops::gateway {
namespace {

std::unique_ptr<brpc::Channel> createChannel(const std::string& addr, int timeout_ms) {
    auto channel = std::make_unique<brpc::Channel>();
    brpc::ChannelOptions options;
    options.protocol = "baidu_std";
    options.timeout_ms = timeout_ms;
    if (channel->Init(addr.c_str(), &options) != 0) {
        ERR("device-gateway failed to initialize downstream channel: addr={}", addr);
        return nullptr;
    }
    return channel;
}

std::string jsonToString(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string getString(const Json::Value& root, const std::string& name, const std::string& fallback = "") {
    if (root.isObject() && root.isMember(name) && root[name].isString()) {
        return root[name].asString();
    }
    return fallback;
}

double getDouble(const Json::Value& root, const std::string& name, double fallback = 0.0) {
    if (root.isObject() && root.isMember(name) && root[name].isNumeric()) {
        return root[name].asDouble();
    }
    return fallback;
}

bool getBool(const Json::Value& root, const std::string& name, bool fallback = false) {
    if (root.isObject() && root.isMember(name) && root[name].isBool()) {
        return root[name].asBool();
    }
    return fallback;
}

deviceops::common::EventSeverity parseSeverity(const Json::Value& payload) {
    const std::string severity = lowerString(getString(payload, "severity", "warning"));
    if (severity == "info") {
        return deviceops::common::EVENT_SEVERITY_INFO;
    }
    if (severity == "critical" || severity == "error" || severity == "fatal") {
        return deviceops::common::EVENT_SEVERITY_CRITICAL;
    }
    return deviceops::common::EVENT_SEVERITY_WARNING;
}

bool isOkResponse(const deviceops::common::CommonResponse& response, std::string* error) {
    if (response.code() == 0) {
        return true;
    }
    if (error != nullptr) {
        *error = response.message();
    }
    return false;
}

void setMetrics(const Json::Value& payload, google::protobuf::Map<std::string, double>* metrics) {
    if (!payload.isObject() || !payload.isMember("metrics") || !payload["metrics"].isObject()) {
        return;
    }
    const Json::Value& source = payload["metrics"];
    for (const auto& name : source.getMemberNames()) {
        if (source[name].isNumeric()) {
            (*metrics)[name] = source[name].asDouble();
        }
    }
}

} // namespace

DownstreamForwarder::DownstreamForwarder(DownstreamRpcConfig config)
    : _config(std::move(config)) {
    if (!_config.enabled) {
        INF("device-gateway downstream forwarding disabled");
        return;
    }

    _device_channel = createChannel(_config.device_addr, _config.timeout_ms);
    _telemetry_channel = createChannel(_config.telemetry_addr, _config.timeout_ms);
    _event_channel = createChannel(_config.event_addr, _config.timeout_ms);
    _log_channel = createChannel(_config.log_addr, _config.timeout_ms);

    if (_device_channel) {
        _device_stub = std::make_unique<deviceops::device::DeviceService_Stub>(_device_channel.get());
    }
    if (_telemetry_channel) {
        _telemetry_stub = std::make_unique<deviceops::telemetry::TelemetryService_Stub>(_telemetry_channel.get());
    }
    if (_event_channel) {
        _event_stub = std::make_unique<deviceops::event::EventService_Stub>(_event_channel.get());
    }
    if (_log_channel) {
        _log_stub = std::make_unique<deviceops::log::LogService_Stub>(_log_channel.get());
    }
}

DownstreamForwarder::~DownstreamForwarder() = default;

bool DownstreamForwarder::forward(const ParsedMqttMessage& message, std::string* error) {
    if (!_config.enabled) {
        return true;
    }
    if (message.message_type == "register") {
        return forwardRegister(message, error);
    }
    if (message.message_type == "telemetry") {
        return forwardTelemetry(message, error);
    }
    if (message.message_type == "alarm") {
        return forwardAlarm(message, error);
    }
    if (message.message_type == "log") {
        return forwardLog(message, error);
    }
    if (message.message_type == "heartbeat") {
        return true;
    }

    if (error != nullptr) {
        *error = "unsupported message type: " + message.message_type;
    }
    return false;
}

bool DownstreamForwarder::forwardRegister(const ParsedMqttMessage& message, std::string* error) {
    if (!_device_stub) {
        if (error != nullptr) {
            *error = "device downstream channel is not ready";
        }
        return false;
    }

    deviceops::device::CreateDeviceRequest create_request;
    create_request.set_device_id(message.device_id);
    create_request.set_device_name(getString(message.payload, "device_name", message.device_id));
    create_request.set_device_type(getString(message.payload, "device_type", "device"));
    create_request.set_model(getString(message.payload, "model"));
    create_request.set_manufacturer(getString(message.payload, "manufacturer"));
    create_request.set_location(getString(message.payload, "location"));
    create_request.set_access_token(getString(message.payload, "access_token"));
    create_request.set_protocol("mqtt");
    create_request.set_description(getString(message.payload, "description"));

    brpc::Controller create_controller;
    deviceops::device::CreateDeviceResponse create_response;
    _device_stub->CreateDevice(&create_controller, &create_request, &create_response, nullptr);
    if (create_controller.Failed()) {
        if (error != nullptr) {
            *error = create_controller.ErrorText();
        }
        return false;
    }
    if (create_response.response().code() == 0) {
        return true;
    }
    if (create_response.response().code() != 409) {
        return isOkResponse(create_response.response(), error);
    }

    deviceops::device::VerifyDeviceAccessRequest verify_request;
    verify_request.set_device_id(message.device_id);
    verify_request.set_access_token(getString(message.payload, "access_token"));
    verify_request.set_protocol("mqtt");

    brpc::Controller verify_controller;
    deviceops::device::VerifyDeviceAccessResponse verify_response;
    _device_stub->VerifyDeviceAccess(&verify_controller, &verify_request, &verify_response, nullptr);
    if (verify_controller.Failed()) {
        if (error != nullptr) {
            *error = verify_controller.ErrorText();
        }
        return false;
    }
    if (!isOkResponse(verify_response.response(), error)) {
        return false;
    }
    if (!verify_response.allowed()) {
        if (error != nullptr) {
            *error = "device access denied";
        }
        return false;
    }
    return true;
}

bool DownstreamForwarder::forwardTelemetry(const ParsedMqttMessage& message, std::string* error) {
    if (!_telemetry_stub) {
        if (error != nullptr) {
            *error = "telemetry downstream channel is not ready";
        }
        return false;
    }

    deviceops::telemetry::UploadTelemetryRequest request;
    request.set_trace_id(message.message_id);
    auto* telemetry = request.mutable_telemetry();
    telemetry->set_device_id(message.device_id);
    telemetry->set_online(getBool(message.payload, "online", true));
    telemetry->set_battery(getDouble(message.payload, "battery"));
    telemetry->set_temperature(getDouble(message.payload, "temperature"));
    telemetry->set_speed(getDouble(message.payload, "speed"));
    telemetry->set_run_mode(getString(message.payload, "run_mode"));
    telemetry->set_error_code(getString(message.payload, "error_code"));
    telemetry->set_reported_at(message.timestamp);
    setMetrics(message.payload, telemetry->mutable_metrics());

    brpc::Controller controller;
    deviceops::telemetry::UploadTelemetryResponse response;
    _telemetry_stub->UploadTelemetry(&controller, &request, &response, nullptr);
    if (controller.Failed()) {
        if (error != nullptr) {
            *error = controller.ErrorText();
        }
        return false;
    }
    return isOkResponse(response.response(), error);
}

bool DownstreamForwarder::forwardAlarm(const ParsedMqttMessage& message, std::string* error) {
    if (!_event_stub) {
        if (error != nullptr) {
            *error = "event downstream channel is not ready";
        }
        return false;
    }

    deviceops::event::CreateEventRequest request;
    request.set_trace_id(message.message_id);
    request.set_device_id(message.device_id);
    request.set_event_type(getString(message.payload, "event_type", getString(message.payload, "alarm_type", "alarm")));
    request.set_severity(parseSeverity(message.payload));
    request.set_error_code(getString(message.payload, "error_code"));
    request.set_title(getString(message.payload, "title", "device alarm"));
    request.set_description(getString(message.payload, "description", getString(message.payload, "message")));
    request.set_rule_name(getString(message.payload, "rule_name"));
    request.set_occurred_at(message.timestamp);
    if (message.payload.isMember("metric_snapshot_json") && message.payload["metric_snapshot_json"].isString()) {
        request.set_metric_snapshot_json(message.payload["metric_snapshot_json"].asString());
    } else if (message.payload.isMember("metrics")) {
        request.set_metric_snapshot_json(jsonToString(message.payload["metrics"]));
    } else {
        request.set_metric_snapshot_json(jsonToString(message.payload));
    }

    brpc::Controller controller;
    deviceops::event::CreateEventResponse response;
    _event_stub->CreateEvent(&controller, &request, &response, nullptr);
    if (controller.Failed()) {
        if (error != nullptr) {
            *error = controller.ErrorText();
        }
        return false;
    }
    return isOkResponse(response.response(), error);
}

bool DownstreamForwarder::forwardLog(const ParsedMqttMessage& message, std::string* error) {
    if (!_log_stub) {
        if (error != nullptr) {
            *error = "log downstream channel is not ready";
        }
        return false;
    }

    deviceops::log::WriteLogRequest request;
    auto* log = request.mutable_log();
    log->set_log_id(getString(message.payload, "log_id", message.message_id));
    log->set_trace_id(message.message_id);
    log->set_device_id(message.device_id);
    log->set_service_name(getString(message.payload, "service_name", "device-gateway"));
    log->set_source_type(getString(message.payload, "source_type", "device"));
    log->set_level(getString(message.payload, "level", "info"));
    log->set_message(getString(message.payload, "message", jsonToString(message.payload)));
    log->set_error_code(getString(message.payload, "error_code"));
    log->set_event_id(getString(message.payload, "event_id"));
    log->set_timestamp(message.timestamp);

    if (message.payload.isMember("tags") && message.payload["tags"].isArray()) {
        for (const auto& tag : message.payload["tags"]) {
            if (tag.isString()) {
                log->add_tags(tag.asString());
            }
        }
    }
    if (message.payload.isMember("context_json") && message.payload["context_json"].isString()) {
        log->set_context_json(message.payload["context_json"].asString());
    } else if (message.payload.isMember("context")) {
        log->set_context_json(jsonToString(message.payload["context"]));
    } else {
        log->set_context_json(jsonToString(message.payload));
    }

    brpc::Controller controller;
    deviceops::log::WriteLogResponse response;
    _log_stub->WriteLog(&controller, &request, &response, nullptr);
    if (controller.Failed()) {
        if (error != nullptr) {
            *error = controller.ErrorText();
        }
        return false;
    }
    return isOkResponse(response.response(), error);
}

} // namespace deviceops::gateway
