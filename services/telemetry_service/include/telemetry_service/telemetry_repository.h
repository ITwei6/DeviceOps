#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "telemetry.pb.h"

namespace deviceops::telemetry_service {

struct RedisConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string user = "default";
    std::string password;
    int db = 0;
};

struct ListTelemetryFilter {
    int page = 1;
    int page_size = 20;
    std::vector<std::string> device_ids;
    bool only_online = false;
};

struct QueryHistoryFilter {
    std::string device_id;
    int64_t start_time = 0;
    int64_t end_time = 0;
    int page = 1;
    int page_size = 20;
};

class TelemetryRepository {
public:
    explicit TelemetryRepository(RedisConfig redis_config);

    void upload(const deviceops::telemetry::DeviceTelemetry& telemetry);
    std::optional<deviceops::telemetry::DeviceTelemetry> getRealtime(const std::string& device_id) const;
    std::vector<deviceops::telemetry::DeviceTelemetry> listRealtime(const ListTelemetryFilter& filter, int64_t* total) const;
    std::vector<deviceops::telemetry::DeviceTelemetry> queryHistory(const QueryHistoryFilter& filter, int64_t* total) const;

    bool redisEnabled() const;

private:
    struct RedisHolder;

    void writeRedis(const deviceops::telemetry::DeviceTelemetry& telemetry);
    static std::string statusKey(const std::string& device_id);
    static std::string onlineKey(const std::string& device_id);
    static std::string toJson(const deviceops::telemetry::DeviceTelemetry& telemetry);

private:
    RedisConfig _redis_config;
    std::shared_ptr<RedisHolder> _redis;
    mutable std::mutex _mutex;
    std::map<std::string, deviceops::telemetry::DeviceTelemetry> _latest;
    std::map<std::string, std::vector<deviceops::telemetry::DeviceTelemetry>> _history;
};

RedisConfig loadRedisConfigFromEnv();
int64_t currentUnixMillis();

} // namespace deviceops::telemetry_service
