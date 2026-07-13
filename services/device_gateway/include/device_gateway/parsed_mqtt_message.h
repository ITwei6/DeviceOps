#pragma once

#include <jsoncpp/json/json.h>

#include <cstdint>
#include <string>

namespace deviceops::gateway {

struct ParsedMqttMessage {
    std::string message_type;
    std::string device_id;
    std::string message_id;
    int64_t timestamp = 0;
    Json::Value payload;
};

} // namespace deviceops::gateway
