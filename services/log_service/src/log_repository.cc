#include "log_service/log_repository.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>

#include <jsoncpp/json/json.h>

#include "elastic.h"

namespace deviceops::log_service {
namespace {

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string getenvOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

size_t getenvSizeOrDefault(const char* name, size_t fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return static_cast<size_t>(std::stoul(value));
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> splitHosts(const std::string& hosts) {
    std::vector<std::string> result;
    std::stringstream ss(hosts);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

void appendTags(tewics::Inserter* inserter, const deviceops::log::LogEntry& log) {
    for (const auto& tag : log.tags()) {
        inserter->append("tags", tag);
    }
}

void addTagsFromJson(const Json::Value& value, deviceops::log::LogEntry* log) {
    if (!value.isMember("tags") || !value["tags"].isArray()) {
        return;
    }
    for (const auto& tag : value["tags"]) {
        log->add_tags(tag.asString());
    }
}

bool inTimeRange(int64_t timestamp, int64_t start_time, int64_t end_time) {
    if (start_time > 0 && timestamp < start_time) {
        return false;
    }
    if (end_time > 0 && timestamp > end_time) {
        return false;
    }
    return true;
}

} // namespace

LogStoreConfig loadLogStoreConfigFromEnv() {
    LogStoreConfig config;
    const auto hosts = splitHosts(getenvOrDefault("DEVICEOPS_ES_HOSTS", ""));
    if (!hosts.empty()) {
        config.hosts = hosts;
    } else {
        config.hosts = {getenvOrDefault("DEVICEOPS_ES_URL", config.hosts.front())};
    }
    config.index_prefix = getenvOrDefault("DEVICEOPS_LOG_INDEX_PREFIX", config.index_prefix);
    config.max_query_size = getenvSizeOrDefault("DEVICEOPS_LOG_QUERY_SIZE", config.max_query_size);
    return config;
}

LogRepository::LogRepository(LogStoreConfig config)
    : _config(std::move(config)),
      _client(std::make_shared<tewics::ESClient>(_config.hosts)) {
}

bool LogRepository::write(const deviceops::log::LogEntry& input, std::string* error) {
    if (input.message().empty()) {
        if (error != nullptr) {
            *error = "message is required";
        }
        return false;
    }

    const int64_t now = currentUnixMillis();
    const auto log = normalize(input, nextLogId(now), now);
    if (!insertToElastic(log, now, error)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _recent.push_back(log);
    if (_recent.size() > _config.max_query_size) {
        _recent.erase(_recent.begin(), _recent.begin() + static_cast<std::ptrdiff_t>(_recent.size() - _config.max_query_size));
    }
    return true;
}

std::vector<deviceops::log::LogEntry> LogRepository::query(const LogQueryFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    auto matched = searchElastic(filter);
    const auto recent = recentSnapshot();
    for (const auto& log : recent) {
        if (matches(log, filter)) {
            const auto exists = std::any_of(matched.begin(), matched.end(), [&](const auto& item) {
                return item.log_id() == log.log_id();
            });
            if (!exists) {
                matched.push_back(log);
            }
        }
    }

    std::sort(matched.begin(), matched.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.timestamp() > rhs.timestamp();
    });

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }

    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<deviceops::log::LogEntry>(matched.begin() + begin, matched.begin() + end);
}

std::vector<deviceops::log::LogEntry> LogRepository::context(const std::string& device_id, int64_t center_time, int64_t before_ms, int64_t after_ms, int limit) const {
    LogQueryFilter filter;
    filter.device_id = device_id;
    filter.start_time = center_time - std::max<int64_t>(before_ms, 0);
    filter.end_time = center_time + std::max<int64_t>(after_ms, 0);
    filter.page = 1;
    filter.page_size = limit <= 0 ? 50 : std::min(limit, 200);

    int64_t total = 0;
    auto logs = query(filter, &total);
    std::sort(logs.begin(), logs.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.timestamp() < rhs.timestamp();
    });
    return logs;
}

std::string LogRepository::indexNameForTimestamp(int64_t timestamp) const {
    const std::time_t seconds = static_cast<std::time_t>(timestamp / 1000);
    std::tm tm{};
    gmtime_r(&seconds, &tm);

    std::ostringstream oss;
    oss << _config.index_prefix << "-" << std::put_time(&tm, "%Y.%m");
    return oss.str();
}

std::string LogRepository::nextLogId(int64_t timestamp) {
    std::lock_guard<std::mutex> lock(_mutex);
    std::ostringstream oss;
    oss << "log-" << timestamp << "-" << _sequence++;
    return oss.str();
}

bool LogRepository::insertToElastic(const deviceops::log::LogEntry& log, int64_t ingested_at, std::string* error) {
    try {
        const auto index_name = indexNameForTimestamp(log.timestamp());
        tewics::Inserter inserter(index_name, log.log_id());
        inserter.add("log_id", log.log_id());
        inserter.add("trace_id", log.trace_id());
        inserter.add("device_id", log.device_id());
        inserter.add("service_name", log.service_name());
        inserter.add("source_type", log.source_type());
        inserter.add("level", log.level());
        inserter.add("message", log.message());
        inserter.add("error_code", log.error_code());
        inserter.add("event_id", log.event_id());
        appendTags(&inserter, log);
        inserter.add("context_json", log.context_json());
        inserter.add("timestamp", static_cast<Json::Int64>(log.timestamp()));
        inserter.add("ingested_at", static_cast<Json::Int64>(ingested_at));
        if (!_client->insert(inserter)) {
            if (error != nullptr) {
                *error = "failed to write log to elasticsearch";
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

std::vector<deviceops::log::LogEntry> LogRepository::searchElastic(const LogQueryFilter& filter) const {
    std::vector<deviceops::log::LogEntry> result;
    try {
        const int64_t index_time = filter.start_time > 0 ? filter.start_time : currentUnixMillis();
        tewics::Searcher searcher(indexNameForTimestamp(index_time));
        searcher.size(_config.max_query_size);
        searcher.from(0);

        auto query = searcher.query();
        auto must = query->must();
        if (!filter.device_id.empty()) {
            must->match("device_id")->setValue(filter.device_id);
        }
        if (!filter.service_name.empty()) {
            must->match("service_name")->setValue(filter.service_name);
        }
        if (!filter.level.empty()) {
            must->match("level")->setValue(filter.level);
        }
        if (!filter.event_id.empty()) {
            must->match("event_id")->setValue(filter.event_id);
        }
        if (filter.start_time > 0 || filter.end_time > 0) {
            must->range("timestamp")->setRange(filter.start_time, filter.end_time > 0 ? filter.end_time : currentUnixMillis());
        }
        if (!filter.keyword.empty()) {
            auto multi_match = must->multi_match();
            multi_match->setQuery(filter.keyword);
            multi_match->appendField("message");
            multi_match->appendField("error_code");
            multi_match->appendField("context_json");
        }
        if (filter.device_id.empty() && filter.service_name.empty() && filter.level.empty()
            && filter.event_id.empty() && filter.start_time <= 0 && filter.end_time <= 0 && filter.keyword.empty()) {
            query->match_all();
        }

        const auto response = _client->search(searcher);
        if (!response.has_value()) {
            return result;
        }
        for (const auto& item : *response) {
            auto log = fromJson(item);
            if (matches(log, filter)) {
                result.push_back(log);
            }
        }
    } catch (const std::exception&) {
        result.clear();
    }
    return result;
}

std::vector<deviceops::log::LogEntry> LogRepository::recentSnapshot() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _recent;
}

deviceops::log::LogEntry LogRepository::normalize(const deviceops::log::LogEntry& input, const std::string& generated_id, int64_t now) {
    auto log = input;
    if (log.log_id().empty()) {
        log.set_log_id(generated_id);
    }
    if (log.level().empty()) {
        log.set_level("info");
    }
    if (log.source_type().empty()) {
        log.set_source_type("service");
    }
    if (log.timestamp() <= 0) {
        log.set_timestamp(now);
    }
    return log;
}

bool LogRepository::matches(const deviceops::log::LogEntry& log, const LogQueryFilter& filter) {
    if (!filter.device_id.empty() && log.device_id() != filter.device_id) {
        return false;
    }
    if (!filter.service_name.empty() && log.service_name() != filter.service_name) {
        return false;
    }
    if (!filter.level.empty() && log.level() != filter.level) {
        return false;
    }
    if (!filter.event_id.empty() && log.event_id() != filter.event_id) {
        return false;
    }
    if (!inTimeRange(log.timestamp(), filter.start_time, filter.end_time)) {
        return false;
    }
    if (!filter.keyword.empty()
        && !contains(log.message(), filter.keyword)
        && !contains(log.error_code(), filter.keyword)
        && !contains(log.context_json(), filter.keyword)) {
        return false;
    }
    return true;
}

deviceops::log::LogEntry LogRepository::fromJson(const Json::Value& value) {
    deviceops::log::LogEntry log;
    log.set_log_id(value["log_id"].asString());
    log.set_trace_id(value["trace_id"].asString());
    log.set_device_id(value["device_id"].asString());
    log.set_service_name(value["service_name"].asString());
    log.set_source_type(value["source_type"].asString());
    log.set_level(value["level"].asString());
    log.set_message(value["message"].asString());
    log.set_error_code(value["error_code"].asString());
    log.set_event_id(value["event_id"].asString());
    addTagsFromJson(value, &log);
    log.set_context_json(value["context_json"].asString());
    log.set_timestamp(value["timestamp"].asInt64());
    return log;
}

bool LogRepository::contains(const std::string& value, const std::string& keyword) {
    return keyword.empty() || value.find(keyword) != std::string::npos;
}

int LogRepository::normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int LogRepository::normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return page_size > 100 ? 100 : page_size;
}

} // namespace deviceops::log_service
