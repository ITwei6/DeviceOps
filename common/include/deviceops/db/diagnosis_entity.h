#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace deviceops::db {

#pragma db object table("fault_records")
struct FaultRecordEntity {
    #pragma db id auto column("id")
    uint64_t id = 0;

    #pragma db unique column("fault_id") type("VARCHAR(128)")
    std::string fault_id;

    #pragma db column("device_id") type("VARCHAR(128)")
    std::string device_id;

    #pragma db column("event_id") type("VARCHAR(128)") null
    odb::nullable<std::string> event_id;

    #pragma db column("owner_user_id")
    uint64_t owner_user_id = 0;

    #pragma db column("fault_type") type("VARCHAR(128)")
    std::string fault_type;

    #pragma db column("severity") type("VARCHAR(32)")
    std::string severity;

    #pragma db column("status") type("VARCHAR(32)")
    std::string status = "new";

    #pragma db column("symptom") type("LONGTEXT")
    std::string symptom;

    #pragma db column("root_cause") type("LONGTEXT") null
    odb::nullable<std::string> root_cause;

    #pragma db column("solution") type("LONGTEXT") null
    odb::nullable<std::string> solution;

    #pragma db column("started_at")
    int64_t started_at = 0;

    #pragma db column("resolved_at") null
    odb::nullable<int64_t> resolved_at;

    #pragma db column("created_at")
    int64_t created_at = 0;

    #pragma db column("updated_at")
    int64_t updated_at = 0;
};

#pragma db object table("diagnosis_reports")
struct DiagnosisReportEntity {
    #pragma db id auto column("id")
    uint64_t id = 0;

    #pragma db unique column("report_id") type("VARCHAR(128)")
    std::string report_id;

    #pragma db column("device_id") type("VARCHAR(128)")
    std::string device_id;

    #pragma db column("event_id") type("VARCHAR(128)") null
    odb::nullable<std::string> event_id;

    #pragma db column("fault_id") type("VARCHAR(128)") null
    odb::nullable<std::string> fault_id;

    #pragma db column("created_by")
    uint64_t created_by = 0;

    #pragma db column("report_type") type("VARCHAR(32)")
    std::string report_type = "ai";

    #pragma db column("status") type("VARCHAR(32)")
    std::string status = "draft";

    #pragma db column("summary") type("LONGTEXT")
    std::string summary;

    #pragma db column("possible_causes_json") type("LONGTEXT") null
    odb::nullable<std::string> possible_causes_json;

    #pragma db column("recommended_actions_json") type("LONGTEXT") null
    odb::nullable<std::string> recommended_actions_json;

    #pragma db column("evidence_json") type("LONGTEXT") null
    odb::nullable<std::string> evidence_json;

    #pragma db column("ai_model") type("VARCHAR(128)") null
    odb::nullable<std::string> ai_model;

    #pragma db column("confidence")
    double confidence = 0.0;

    #pragma db column("created_at")
    int64_t created_at = 0;

    #pragma db column("updated_at")
    int64_t updated_at = 0;
};

} // namespace deviceops::db
