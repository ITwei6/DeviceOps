#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace deviceops::db {

#pragma db object table("events")
struct EventEntity {
    #pragma db id auto column("id")
    uint64_t id = 0;

    #pragma db unique column("event_id") type("VARCHAR(128)")
    std::string event_id;

    #pragma db column("device_id")
    std::string device_id;

    #pragma db column("event_type")
    std::string event_type;

    #pragma db column("severity")
    std::string severity;

    #pragma db column("status")
    std::string status = "open";

    #pragma db column("error_code") null
    odb::nullable<std::string> error_code;

    #pragma db column("title")
    std::string title;

    #pragma db column("description") null
    odb::nullable<std::string> description;

    #pragma db column("metric_snapshot_json") null
    odb::nullable<std::string> metric_snapshot_json;

    #pragma db column("rule_name") null
    odb::nullable<std::string> rule_name;

    #pragma db column("occurred_at")
    int64_t occurred_at = 0;

    #pragma db column("resolved_at") null
    odb::nullable<int64_t> resolved_at;

    #pragma db column("created_at")
    int64_t created_at = 0;

    #pragma db column("updated_at")
    int64_t updated_at = 0;
};

} // namespace deviceops::db
