#include "telemetry_service/telemetry_repository.h"

#include <jsoncpp/json/json.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>

#include <sw/redis++/redis.h>

#include "log.h"
#include "redis.h"

namespace deviceops::telemetry_service {
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
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "on";
}

int normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return std::min(page_size, 100);
}

template <typename T>
std::vector<T> pageSlice(const std::vector<T>& values, int page, int page_size) {
    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= values.size()) {
        return {};
    }
    const size_t end = std::min(values.size(), begin + static_cast<size_t>(page_size));
    return std::vector<T>(values.begin() + begin, values.begin() + end);
}

} // namespace

struct TelemetryRepository::RedisHolder {
    explicit RedisHolder(std::shared_ptr<sw::redis::Redis> client)
        : redis(std::move(client)) {
    }

    std::shared_ptr<sw::redis::Redis> redis;
};

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

RedisConfig loadRedisConfigFromEnv() {
    RedisConfig config;
    config.enabled = getenvBoolOrDefault("DEVICEOPS_REDIS_ENABLED", false);
    config.host = getenvOrDefault("DEVICEOPS_REDIS_HOST", config.host);
    config.port = getenvIntOrDefault("DEVICEOPS_REDIS_PORT", config.port);
    config.user = getenvOrDefault("DEVICEOPS_REDIS_USER", config.user);
    config.password = getenvOrDefault("DEVICEOPS_REDIS_PASSWORD", "");
    config.db = getenvIntOrDefault("DEVICEOPS_REDIS_DB", config.db);
    return config;
}

TelemetryRepository::TelemetryRepository(RedisConfig redis_config)
    : _redis_config(std::move(redis_config)) {
    if (!_redis_config.enabled) {
        INF("telemetry-service Redis persistence disabled");
        return;
    }

    try {
        tewredis::redis_settings settings;
        settings.host = _redis_config.host;
        settings.port = _redis_config.port;
        settings.user = _redis_config.user;
        settings.passwd = _redis_config.password;
        settings.db = _redis_config.db;
        auto redis = tewredis::RedisFactory::create(settings);
        redis->ping();
        _redis = std::make_shared<RedisHolder>(redis);
        INF("telemetry-service Redis connected: {}:{}", _redis_config.host, _redis_config.port);
    } catch (const std::exception& e) {
        WRN("telemetry-service Redis disabled after connection failure: {}", e.what());
        _redis.reset();
    }
}

void TelemetryRepository::upload(const deviceops::telemetry::DeviceTelemetry& telemetry) {
    auto normalized = telemetry;
    if (normalized.reported_at() <= 0) {
        normalized.set_reported_at(currentUnixMillis());
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _latest[normalized.device_id()] = normalized;
        _history[normalized.device_id()].push_back(normalized);
    }

    writeRedis(normalized);
}

std::optional<deviceops::telemetry::DeviceTelemetry> TelemetryRepository::getRealtime(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _latest.find(device_id);
    if (it == _latest.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<deviceops::telemetry::DeviceTelemetry> TelemetryRepository::listRealtime(const ListTelemetryFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);
    std::vector<deviceops::telemetry::DeviceTelemetry> matched;

    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& item : _latest) {
        const auto& telemetry = item.second;
        if (!filter.device_ids.empty()
            && std::find(filter.device_ids.begin(), filter.device_ids.end(), telemetry.device_id()) == filter.device_ids.end()) {
            continue;
        }
        if (filter.only_online && !telemetry.online()) {
            continue;
        }
        matched.push_back(telemetry);
    }

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }
    return pageSlice(matched, page, page_size);
}

std::vector<deviceops::telemetry::DeviceTelemetry> TelemetryRepository::queryHistory(const QueryHistoryFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);
    std::vector<deviceops::telemetry::DeviceTelemetry> matched;

    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _history.find(filter.device_id);
    if (it != _history.end()) {
        for (const auto& telemetry : it->second) {
            if (filter.start_time > 0 && telemetry.reported_at() < filter.start_time) {
                continue;
            }
            if (filter.end_time > 0 && telemetry.reported_at() > filter.end_time) {
                continue;
            }
            matched.push_back(telemetry);
        }
    }

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }
    return pageSlice(matched, page, page_size);
}

bool TelemetryRepository::redisEnabled() const {
    return static_cast<bool>(_redis);
}

void TelemetryRepository::writeRedis(const deviceops::telemetry::DeviceTelemetry& telemetry) {
    if (!_redis) {
        return;
    }

    try {
        _redis->redis->set(statusKey(telemetry.device_id()), toJson(telemetry));
        _redis->redis->set(onlineKey(telemetry.device_id()), telemetry.online() ? "1" : "0");
    } catch (const std::exception& e) {
        WRN("telemetry-service Redis write failed: device_id={}, error={}", telemetry.device_id(), e.what());
    }
}

std::string TelemetryRepository::statusKey(const std::string& device_id) {
    return "deviceops:device:status:" + device_id;
}

std::string TelemetryRepository::onlineKey(const std::string& device_id) {
    return "deviceops:device:online:" + device_id;
}

std::string TelemetryRepository::toJson(const deviceops::telemetry::DeviceTelemetry& telemetry) {
    Json::Value root;
    root["device_id"] = telemetry.device_id();
    root["online"] = telemetry.online();
    root["battery"] = telemetry.battery();
    root["temperature"] = telemetry.temperature();
    root["speed"] = telemetry.speed();
    root["run_mode"] = telemetry.run_mode();
    root["error_code"] = telemetry.error_code();
    root["reported_at"] = Json::Int64(telemetry.reported_at());
    for (const auto& item : telemetry.metrics()) {
        root["metrics"][item.first] = item.second;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

} // namespace deviceops::telemetry_service
