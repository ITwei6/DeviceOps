#include "event_service/event_repository.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "deviceops/db/odb_database.h"
#include "event_entity-odb.hxx"

namespace deviceops::event_service {
namespace {

int normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return std::min(page_size, 100);
}

std::string nullableString(const odb::nullable<std::string>& value) {
    return value ? value.get() : "";
}

int64_t nullableInt64(const odb::nullable<int64_t>& value) {
    return value ? value.get() : 0;
}

bool inTimeRange(int64_t occurred_at, int64_t start_time, int64_t end_time) {
    if (start_time > 0 && occurred_at < start_time) {
        return false;
    }
    if (end_time > 0 && occurred_at > end_time) {
        return false;
    }
    return true;
}

} // namespace

EventRepository::EventRepository(std::shared_ptr<odb::database> database)
    : _database(std::move(database)) {
}

bool EventRepository::create(const deviceops::event::CreateEventRequest& request, deviceops::event::Event* created, std::string* error) {
    if (request.device_id().empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }
    if (request.event_type().empty()) {
        if (error != nullptr) {
            *error = "event_type is required";
        }
        return false;
    }
    if (request.title().empty()) {
        if (error != nullptr) {
            *error = "title is required";
        }
        return false;
    }
    if (request.severity() == deviceops::common::EVENT_SEVERITY_UNSPECIFIED) {
        if (error != nullptr) {
            *error = "severity is required";
        }
        return false;
    }

    try {
        const int64_t now = deviceops::db::currentUnixMillis();
        deviceops::db::EventEntity entity;
        entity.event_id = nextEventId(now);
        entity.device_id = request.device_id();
        entity.event_type = request.event_type();
        entity.severity = severityToString(request.severity());
        entity.status = "open";
        if (!request.error_code().empty()) {
            entity.error_code = request.error_code();
        }
        entity.title = request.title();
        if (!request.description().empty()) {
            entity.description = request.description();
        }
        if (!request.metric_snapshot_json().empty()) {
            entity.metric_snapshot_json = request.metric_snapshot_json();
        }
        if (!request.rule_name().empty()) {
            entity.rule_name = request.rule_name();
        }
        entity.occurred_at = request.occurred_at() > 0 ? request.occurred_at() : now;
        entity.created_at = now;
        entity.updated_at = now;

        odb::transaction tx(_database->begin());
        _database->persist(entity);
        tx.commit();

        if (created != nullptr) {
            *created = toProto(entity);
        }
        return true;
    } catch (const odb::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

std::optional<deviceops::event::Event> EventRepository::get(const std::string& event_id) const {
    try {
        using Query = odb::query<deviceops::db::EventEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::EventEntity>(Query::event_id == event_id);
        tx.commit();
        if (!entity) {
            return std::nullopt;
        }
        return toProto(*entity);
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<deviceops::event::Event> EventRepository::list(const ListEventFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<deviceops::event::Event> matched;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::EventEntity>;
        Result result(_database->query<deviceops::db::EventEntity>());
        for (const auto& entity : result) {
            const auto event = toProto(entity);
            if (!filter.device_id.empty() && event.device_id() != filter.device_id) {
                continue;
            }
            if (!filter.event_type.empty() && event.event_type() != filter.event_type) {
                continue;
            }
            if (filter.severity != deviceops::common::EVENT_SEVERITY_UNSPECIFIED && event.severity() != filter.severity) {
                continue;
            }
            if (filter.status != deviceops::common::EVENT_STATUS_UNSPECIFIED && event.status() != filter.status) {
                continue;
            }
            if (!inTimeRange(event.occurred_at(), filter.start_time, filter.end_time)) {
                continue;
            }
            matched.push_back(event);
        }
        tx.commit();
    } catch (const odb::exception&) {
        matched.clear();
    }

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }

    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<deviceops::event::Event>(matched.begin() + begin, matched.begin() + end);
}

bool EventRepository::updateStatus(const std::string& event_id, deviceops::common::EventStatus status, deviceops::event::Event* updated, std::string* error) {
    if (event_id.empty()) {
        if (error != nullptr) {
            *error = "event_id is required";
        }
        return false;
    }
    if (status == deviceops::common::EVENT_STATUS_UNSPECIFIED) {
        if (error != nullptr) {
            *error = "status is required";
        }
        return false;
    }

    try {
        using Query = odb::query<deviceops::db::EventEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::EventEntity>(Query::event_id == event_id);
        if (!entity) {
            tx.commit();
            if (error != nullptr) {
                *error = "event not found";
            }
            return false;
        }

        const int64_t now = deviceops::db::currentUnixMillis();
        entity->status = statusToString(status);
        entity->updated_at = now;
        if (status == deviceops::common::EVENT_STATUS_RESOLVED || status == deviceops::common::EVENT_STATUS_CLOSED) {
            entity->resolved_at = now;
        }
        _database->update(*entity);
        tx.commit();

        if (updated != nullptr) {
            *updated = toProto(*entity);
        }
        return true;
    } catch (const odb::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

std::string EventRepository::nextEventId(int64_t now) {
    std::ostringstream oss;
    oss << "evt-" << now << "-" << _sequence.fetch_add(1);
    return oss.str();
}

deviceops::event::Event EventRepository::toProto(const deviceops::db::EventEntity& entity) {
    deviceops::event::Event event;
    event.set_id(entity.id);
    event.set_event_id(entity.event_id);
    event.set_device_id(entity.device_id);
    event.set_event_type(entity.event_type);
    event.set_severity(severityFromString(entity.severity));
    event.set_status(statusFromString(entity.status));
    event.set_error_code(nullableString(entity.error_code));
    event.set_title(entity.title);
    event.set_description(nullableString(entity.description));
    event.set_metric_snapshot_json(nullableString(entity.metric_snapshot_json));
    event.set_rule_name(nullableString(entity.rule_name));
    event.set_occurred_at(entity.occurred_at);
    event.set_resolved_at(nullableInt64(entity.resolved_at));
    event.set_created_at(entity.created_at);
    event.set_updated_at(entity.updated_at);
    return event;
}

deviceops::common::EventSeverity EventRepository::severityFromString(const std::string& severity) {
    if (severity == "info") {
        return deviceops::common::EVENT_SEVERITY_INFO;
    }
    if (severity == "warning") {
        return deviceops::common::EVENT_SEVERITY_WARNING;
    }
    if (severity == "critical") {
        return deviceops::common::EVENT_SEVERITY_CRITICAL;
    }
    return deviceops::common::EVENT_SEVERITY_UNSPECIFIED;
}

std::string EventRepository::severityToString(deviceops::common::EventSeverity severity) {
    switch (severity) {
    case deviceops::common::EVENT_SEVERITY_INFO:
        return "info";
    case deviceops::common::EVENT_SEVERITY_WARNING:
        return "warning";
    case deviceops::common::EVENT_SEVERITY_CRITICAL:
        return "critical";
    default:
        return "unspecified";
    }
}

deviceops::common::EventStatus EventRepository::statusFromString(const std::string& status) {
    if (status == "open") {
        return deviceops::common::EVENT_STATUS_OPEN;
    }
    if (status == "processing") {
        return deviceops::common::EVENT_STATUS_PROCESSING;
    }
    if (status == "resolved") {
        return deviceops::common::EVENT_STATUS_RESOLVED;
    }
    if (status == "closed") {
        return deviceops::common::EVENT_STATUS_CLOSED;
    }
    return deviceops::common::EVENT_STATUS_UNSPECIFIED;
}

std::string EventRepository::statusToString(deviceops::common::EventStatus status) {
    switch (status) {
    case deviceops::common::EVENT_STATUS_OPEN:
        return "open";
    case deviceops::common::EVENT_STATUS_PROCESSING:
        return "processing";
    case deviceops::common::EVENT_STATUS_RESOLVED:
        return "resolved";
    case deviceops::common::EVENT_STATUS_CLOSED:
        return "closed";
    default:
        return "unspecified";
    }
}

} // namespace deviceops::event_service
