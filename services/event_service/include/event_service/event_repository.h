#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "event.pb.h"
#include "deviceops/db/event_entity.h"

namespace deviceops::event_service {

struct ListEventFilter {
    int page = 1;
    int page_size = 20;
    std::string device_id;
    std::string event_type;
    deviceops::common::EventSeverity severity = deviceops::common::EVENT_SEVERITY_UNSPECIFIED;
    deviceops::common::EventStatus status = deviceops::common::EVENT_STATUS_UNSPECIFIED;
    int64_t start_time = 0;
    int64_t end_time = 0;
};

class EventRepository {
public:
    explicit EventRepository(std::shared_ptr<odb::database> database);

    bool create(const deviceops::event::CreateEventRequest& request, deviceops::event::Event* created, std::string* error);
    std::optional<deviceops::event::Event> get(const std::string& event_id) const;
    std::vector<deviceops::event::Event> list(const ListEventFilter& filter, int64_t* total) const;
    bool updateStatus(const std::string& event_id, deviceops::common::EventStatus status, deviceops::event::Event* updated, std::string* error);

private:
    std::string nextEventId(int64_t now);
    static deviceops::event::Event toProto(const deviceops::db::EventEntity& entity);
    static deviceops::common::EventSeverity severityFromString(const std::string& severity);
    static std::string severityToString(deviceops::common::EventSeverity severity);
    static deviceops::common::EventStatus statusFromString(const std::string& status);
    static std::string statusToString(deviceops::common::EventStatus status);

private:
    std::shared_ptr<odb::database> _database;
    std::atomic<uint64_t> _sequence{1};
};

} // namespace deviceops::event_service
