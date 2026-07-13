#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "deviceops/db/diagnosis_entity.h"
#include "diagnosis.pb.h"
#include "diagnosis_service/diagnosis_rag_client.h"

namespace deviceops::diagnosis_service {

struct FaultListFilter {
    int page = 1;
    int page_size = 20;
    std::string device_id;
    std::string event_id;
    deviceops::common::FaultStatus status = deviceops::common::FAULT_STATUS_UNSPECIFIED;
    int64_t start_time = 0;
    int64_t end_time = 0;
};

struct ReportListFilter {
    int page = 1;
    int page_size = 20;
    std::string device_id;
    std::string event_id;
    std::string fault_id;
    deviceops::common::ReportStatus status = deviceops::common::REPORT_STATUS_UNSPECIFIED;
};

class DiagnosisRepository {
public:
    explicit DiagnosisRepository(std::shared_ptr<odb::database> database);

    bool createFault(const deviceops::diagnosis::CreateFaultRecordRequest& request, deviceops::diagnosis::FaultRecord* created, std::string* error);
    std::optional<deviceops::diagnosis::FaultRecord> getFault(const std::string& fault_id) const;
    std::vector<deviceops::diagnosis::FaultRecord> listFaults(const FaultListFilter& filter, int64_t* total) const;

    bool createDiagnosisReport(const deviceops::diagnosis::StartDiagnosisRequest& request, const DiagnosisDraft& draft, const std::optional<deviceops::diagnosis::FaultRecord>& fault, deviceops::diagnosis::DiagnosisReport* created, std::string* error);
    std::optional<deviceops::diagnosis::DiagnosisReport> getReport(const std::string& report_id) const;
    std::vector<deviceops::diagnosis::DiagnosisReport> listReports(const ReportListFilter& filter, int64_t* total) const;
    bool confirmReport(const deviceops::diagnosis::ConfirmDiagnosisReportRequest& request, deviceops::diagnosis::DiagnosisReport* updated, std::string* error);

private:
    std::string nextFaultId(int64_t now);
    std::string nextReportId(int64_t now);
    static deviceops::diagnosis::FaultRecord toProto(const deviceops::db::FaultRecordEntity& entity);
    static deviceops::diagnosis::DiagnosisReport toProto(const deviceops::db::DiagnosisReportEntity& entity);
    static bool matchesFaultFilter(const deviceops::diagnosis::FaultRecord& fault, const FaultListFilter& filter);
    static bool matchesReportFilter(const deviceops::diagnosis::DiagnosisReport& report, const ReportListFilter& filter);
    static deviceops::common::EventSeverity severityFromString(const std::string& severity);
    static std::string severityToString(deviceops::common::EventSeverity severity);
    static deviceops::common::FaultStatus faultStatusFromString(const std::string& status);
    static std::string faultStatusToString(deviceops::common::FaultStatus status);
    static deviceops::common::ReportStatus reportStatusFromString(const std::string& status);
    static std::string reportStatusToString(deviceops::common::ReportStatus status);
    static std::string nullableString(const odb::nullable<std::string>& value);
    static int64_t nullableInt64(const odb::nullable<int64_t>& value);
    static int normalizePage(int page);
    static int normalizePageSize(int page_size);

private:
    std::shared_ptr<odb::database> _database;
    std::atomic<uint64_t> _fault_sequence{1};
    std::atomic<uint64_t> _report_sequence{1};
};

} // namespace deviceops::diagnosis_service
