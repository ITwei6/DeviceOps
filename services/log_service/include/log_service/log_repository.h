#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "log.pb.h"

namespace Json {
class Value;
} // namespace Json

namespace tewics {
class ESClient;
} // namespace tewics

namespace deviceops::log_service {

struct LogStoreConfig {
    std::vector<std::string> hosts = {"http://elastic:123456@elasticsearch-service:9200/"};
    std::string index_prefix = "deviceops-logs";
    size_t max_query_size = 500;
};

struct LogQueryFilter {
    int page = 1;
    int page_size = 20;
    std::string device_id;
    std::string service_name;
    std::string level;
    std::string keyword;
    std::string event_id;
    int64_t start_time = 0;
    int64_t end_time = 0;
};

LogStoreConfig loadLogStoreConfigFromEnv();

class LogRepository {
public:
    explicit LogRepository(LogStoreConfig config);

    bool write(const deviceops::log::LogEntry& input, std::string* error);
    std::vector<deviceops::log::LogEntry> query(const LogQueryFilter& filter, int64_t* total) const;
    std::vector<deviceops::log::LogEntry> context(const std::string& device_id, int64_t center_time, int64_t before_ms, int64_t after_ms, int limit) const;

private:
    std::string indexNameForTimestamp(int64_t timestamp) const;
    std::string nextLogId(int64_t timestamp);
    bool insertToElastic(const deviceops::log::LogEntry& log, int64_t ingested_at, std::string* error);
    std::vector<deviceops::log::LogEntry> searchElastic(const LogQueryFilter& filter) const;
    std::vector<deviceops::log::LogEntry> recentSnapshot() const;

    static deviceops::log::LogEntry normalize(const deviceops::log::LogEntry& input, const std::string& generated_id, int64_t now);
    static bool matches(const deviceops::log::LogEntry& log, const LogQueryFilter& filter);
    static deviceops::log::LogEntry fromJson(const Json::Value& value);
    static bool contains(const std::string& value, const std::string& keyword);
    static int normalizePage(int page);
    static int normalizePageSize(int page_size);

private:
    LogStoreConfig _config;
    std::shared_ptr<tewics::ESClient> _client;
    mutable std::mutex _mutex;
    std::vector<deviceops::log::LogEntry> _recent;
    uint64_t _sequence = 1;
};

} // namespace deviceops::log_service
