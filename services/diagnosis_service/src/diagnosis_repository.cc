#include "diagnosis_service/diagnosis_repository.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "deviceops/db/odb_database.h"
#include "diagnosis_entity-odb.hxx"

namespace deviceops::diagnosis_service {
namespace {

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

DiagnosisRepository::DiagnosisRepository(std::shared_ptr<odb::database> database)
    : _database(std::move(database)) {
}

bool DiagnosisRepository::createFault(const deviceops::diagnosis::CreateFaultRecordRequest& request, deviceops::diagnosis::FaultRecord* created, std::string* error) {
    if (request.device_id().empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }
    if (request.fault_type().empty()) {
        if (error != nullptr) {
            *error = "fault_type is required";
        }
        return false;
    }
    if (request.symptom().empty()) {
        if (error != nullptr) {
            *error = "symptom is required";
        }
        return false;
    }

    try {
        const int64_t now = deviceops::db::currentUnixMillis();
        deviceops::db::FaultRecordEntity entity;
        entity.fault_id = nextFaultId(now);
        entity.device_id = request.device_id();
        if (!request.event_id().empty()) {
            entity.event_id = request.event_id();
        }
        entity.owner_user_id = request.owner_user_id();
        entity.fault_type = request.fault_type();
        entity.severity = severityToString(request.severity());
        entity.status = "new";
        entity.symptom = request.symptom();
        entity.started_at = request.started_at() > 0 ? request.started_at() : now;
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

std::optional<deviceops::diagnosis::FaultRecord> DiagnosisRepository::getFault(const std::string& fault_id) const {
    try {
        using Query = odb::query<deviceops::db::FaultRecordEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::FaultRecordEntity>(Query::fault_id == fault_id);
        tx.commit();
        if (!entity) {
            return std::nullopt;
        }
        return toProto(*entity);
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<deviceops::diagnosis::FaultRecord> DiagnosisRepository::listFaults(const FaultListFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<deviceops::diagnosis::FaultRecord> matched;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::FaultRecordEntity>;
        Result result(_database->query<deviceops::db::FaultRecordEntity>());
        for (const auto& entity : result) {
            auto fault = toProto(entity);
            if (matchesFaultFilter(fault, filter)) {
                matched.push_back(std::move(fault));
            }
        }
        tx.commit();
    } catch (const odb::exception&) {
        matched.clear();
    }

    std::sort(matched.begin(), matched.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.started_at() > rhs.started_at();
    });

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }
    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<deviceops::diagnosis::FaultRecord>(matched.begin() + begin, matched.begin() + end);
}

bool DiagnosisRepository::createDiagnosisReport(const deviceops::diagnosis::StartDiagnosisRequest& request, const DiagnosisDraft& draft, const std::optional<deviceops::diagnosis::FaultRecord>& fault, deviceops::diagnosis::DiagnosisReport* created, std::string* error) {
    const std::string device_id = !request.device_id().empty() ? request.device_id() : (fault.has_value() ? fault->device_id() : "");
    if (device_id.empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }

    try {
        const int64_t now = deviceops::db::currentUnixMillis();
        deviceops::db::DiagnosisReportEntity entity;
        entity.report_id = nextReportId(now);
        entity.device_id = device_id;
        if (!request.event_id().empty()) {
            entity.event_id = request.event_id();
        } else if (fault.has_value() && !fault->event_id().empty()) {
            entity.event_id = fault->event_id();
        }
        if (!request.fault_id().empty()) {
            entity.fault_id = request.fault_id();
        } else if (fault.has_value()) {
            entity.fault_id = fault->fault_id();
        }
        entity.created_by = request.requested_by();
        entity.report_type = "ai";
        entity.status = "draft";
        entity.summary = draft.summary.empty() ? "Diagnosis draft generated by RAG service." : draft.summary;
        if (!draft.possible_causes_json.empty()) {
            entity.possible_causes_json = draft.possible_causes_json;
        }
        if (!draft.recommended_actions_json.empty()) {
            entity.recommended_actions_json = draft.recommended_actions_json;
        }
        if (!draft.evidence_json.empty()) {
            entity.evidence_json = draft.evidence_json;
        }
        if (!draft.ai_model.empty()) {
            entity.ai_model = draft.ai_model;
        }
        entity.confidence = draft.confidence;
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

std::optional<deviceops::diagnosis::DiagnosisReport> DiagnosisRepository::getReport(const std::string& report_id) const {
    try {
        using Query = odb::query<deviceops::db::DiagnosisReportEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::DiagnosisReportEntity>(Query::report_id == report_id);
        tx.commit();
        if (!entity) {
            return std::nullopt;
        }
        return toProto(*entity);
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<deviceops::diagnosis::DiagnosisReport> DiagnosisRepository::listReports(const ReportListFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<deviceops::diagnosis::DiagnosisReport> matched;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::DiagnosisReportEntity>;
        Result result(_database->query<deviceops::db::DiagnosisReportEntity>());
        for (const auto& entity : result) {
            auto report = toProto(entity);
            if (matchesReportFilter(report, filter)) {
                matched.push_back(std::move(report));
            }
        }
        tx.commit();
    } catch (const odb::exception&) {
        matched.clear();
    }

    std::sort(matched.begin(), matched.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.created_at() > rhs.created_at();
    });

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }
    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<deviceops::diagnosis::DiagnosisReport>(matched.begin() + begin, matched.begin() + end);
}

bool DiagnosisRepository::confirmReport(const deviceops::diagnosis::ConfirmDiagnosisReportRequest& request, deviceops::diagnosis::DiagnosisReport* updated, std::string* error) {
    if (request.report_id().empty()) {
        if (error != nullptr) {
            *error = "report_id is required";
        }
        return false;
    }

    try {
        using Query = odb::query<deviceops::db::DiagnosisReportEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::DiagnosisReportEntity>(Query::report_id == request.report_id());
        if (!entity) {
            tx.commit();
            if (error != nullptr) {
                *error = "report not found";
            }
            return false;
        }
        entity->status = request.confirmed() ? "confirmed" : "rejected";
        entity->updated_at = deviceops::db::currentUnixMillis();
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

std::string DiagnosisRepository::nextFaultId(int64_t now) {
    std::ostringstream oss;
    oss << "fault-" << now << "-" << _fault_sequence.fetch_add(1);
    return oss.str();
}

std::string DiagnosisRepository::nextReportId(int64_t now) {
    std::ostringstream oss;
    oss << "report-" << now << "-" << _report_sequence.fetch_add(1);
    return oss.str();
}

deviceops::diagnosis::FaultRecord DiagnosisRepository::toProto(const deviceops::db::FaultRecordEntity& entity) {
    deviceops::diagnosis::FaultRecord fault;
    fault.set_id(entity.id);
    fault.set_fault_id(entity.fault_id);
    fault.set_device_id(entity.device_id);
    fault.set_event_id(nullableString(entity.event_id));
    fault.set_owner_user_id(entity.owner_user_id);
    fault.set_fault_type(entity.fault_type);
    fault.set_severity(severityFromString(entity.severity));
    fault.set_status(faultStatusFromString(entity.status));
    fault.set_symptom(entity.symptom);
    fault.set_root_cause(nullableString(entity.root_cause));
    fault.set_solution(nullableString(entity.solution));
    fault.set_started_at(entity.started_at);
    fault.set_resolved_at(nullableInt64(entity.resolved_at));
    fault.set_created_at(entity.created_at);
    fault.set_updated_at(entity.updated_at);
    return fault;
}

deviceops::diagnosis::DiagnosisReport DiagnosisRepository::toProto(const deviceops::db::DiagnosisReportEntity& entity) {
    deviceops::diagnosis::DiagnosisReport report;
    report.set_id(entity.id);
    report.set_report_id(entity.report_id);
    report.set_device_id(entity.device_id);
    report.set_event_id(nullableString(entity.event_id));
    report.set_fault_id(nullableString(entity.fault_id));
    report.set_created_by(entity.created_by);
    report.set_report_type(entity.report_type);
    report.set_status(reportStatusFromString(entity.status));
    report.set_summary(entity.summary);
    report.set_possible_causes_json(nullableString(entity.possible_causes_json));
    report.set_recommended_actions_json(nullableString(entity.recommended_actions_json));
    report.set_evidence_json(nullableString(entity.evidence_json));
    report.set_ai_model(nullableString(entity.ai_model));
    report.set_confidence(entity.confidence);
    report.set_created_at(entity.created_at);
    report.set_updated_at(entity.updated_at);
    return report;
}

bool DiagnosisRepository::matchesFaultFilter(const deviceops::diagnosis::FaultRecord& fault, const FaultListFilter& filter) {
    if (!filter.device_id.empty() && fault.device_id() != filter.device_id) {
        return false;
    }
    if (!filter.event_id.empty() && fault.event_id() != filter.event_id) {
        return false;
    }
    if (filter.status != deviceops::common::FAULT_STATUS_UNSPECIFIED && fault.status() != filter.status) {
        return false;
    }
    return inTimeRange(fault.started_at(), filter.start_time, filter.end_time);
}

bool DiagnosisRepository::matchesReportFilter(const deviceops::diagnosis::DiagnosisReport& report, const ReportListFilter& filter) {
    if (!filter.device_id.empty() && report.device_id() != filter.device_id) {
        return false;
    }
    if (!filter.event_id.empty() && report.event_id() != filter.event_id) {
        return false;
    }
    if (!filter.fault_id.empty() && report.fault_id() != filter.fault_id) {
        return false;
    }
    if (filter.status != deviceops::common::REPORT_STATUS_UNSPECIFIED && report.status() != filter.status) {
        return false;
    }
    return true;
}

deviceops::common::EventSeverity DiagnosisRepository::severityFromString(const std::string& severity) {
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

std::string DiagnosisRepository::severityToString(deviceops::common::EventSeverity severity) {
    switch (severity) {
    case deviceops::common::EVENT_SEVERITY_INFO:
        return "info";
    case deviceops::common::EVENT_SEVERITY_WARNING:
        return "warning";
    case deviceops::common::EVENT_SEVERITY_CRITICAL:
        return "critical";
    default:
        return "warning";
    }
}

deviceops::common::FaultStatus DiagnosisRepository::faultStatusFromString(const std::string& status) {
    if (status == "new") {
        return deviceops::common::FAULT_STATUS_NEW;
    }
    if (status == "processing") {
        return deviceops::common::FAULT_STATUS_PROCESSING;
    }
    if (status == "resolved") {
        return deviceops::common::FAULT_STATUS_RESOLVED;
    }
    if (status == "closed") {
        return deviceops::common::FAULT_STATUS_CLOSED;
    }
    return deviceops::common::FAULT_STATUS_UNSPECIFIED;
}

std::string DiagnosisRepository::faultStatusToString(deviceops::common::FaultStatus status) {
    switch (status) {
    case deviceops::common::FAULT_STATUS_NEW:
        return "new";
    case deviceops::common::FAULT_STATUS_PROCESSING:
        return "processing";
    case deviceops::common::FAULT_STATUS_RESOLVED:
        return "resolved";
    case deviceops::common::FAULT_STATUS_CLOSED:
        return "closed";
    default:
        return "new";
    }
}

deviceops::common::ReportStatus DiagnosisRepository::reportStatusFromString(const std::string& status) {
    if (status == "draft") {
        return deviceops::common::REPORT_STATUS_DRAFT;
    }
    if (status == "confirmed") {
        return deviceops::common::REPORT_STATUS_CONFIRMED;
    }
    if (status == "rejected") {
        return deviceops::common::REPORT_STATUS_REJECTED;
    }
    return deviceops::common::REPORT_STATUS_UNSPECIFIED;
}

std::string DiagnosisRepository::reportStatusToString(deviceops::common::ReportStatus status) {
    switch (status) {
    case deviceops::common::REPORT_STATUS_DRAFT:
        return "draft";
    case deviceops::common::REPORT_STATUS_CONFIRMED:
        return "confirmed";
    case deviceops::common::REPORT_STATUS_REJECTED:
        return "rejected";
    default:
        return "draft";
    }
}

std::string DiagnosisRepository::nullableString(const odb::nullable<std::string>& value) {
    return value ? value.get() : "";
}

int64_t DiagnosisRepository::nullableInt64(const odb::nullable<int64_t>& value) {
    return value ? value.get() : 0;
}

int DiagnosisRepository::normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int DiagnosisRepository::normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return page_size > 100 ? 100 : page_size;
}

} // namespace deviceops::diagnosis_service
