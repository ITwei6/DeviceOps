#include "diagnosis_service/diagnosis_service_impl.h"

#include <brpc/server.h>
#include <jsoncpp/json/json.h>

#include "log.h"

namespace deviceops::diagnosis_service {
namespace {

void setResponse(deviceops::common::CommonResponse* response, int code, const std::string& message) {
    response->set_code(code);
    response->set_message(message);
}

int pageOrDefault(const deviceops::common::PageRequest& page) {
    return page.page() <= 0 ? 1 : page.page();
}

int pageSizeOrDefault(const deviceops::common::PageRequest& page) {
    if (page.page_size() <= 0) {
        return 20;
    }
    return page.page_size() > 100 ? 100 : page.page_size();
}

} // namespace

DiagnosisServiceImpl::DiagnosisServiceImpl(DiagnosisRepository* repository,
    DiagnosisRagClient* rag_client,
    deviceops::mq::RabbitMqEventPublisher* event_publisher)
    : _repository(repository),
      _rag_client(rag_client),
      _event_publisher(event_publisher) {
}

void DiagnosisServiceImpl::CreateFaultRecord(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::CreateFaultRecordRequest* request,
    deviceops::diagnosis::CreateFaultRecordResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    deviceops::diagnosis::FaultRecord fault;
    std::string error;
    if (!_repository->createFault(*request, &fault, &error)) {
        setResponse(response->mutable_response(), 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_fault() = fault;
}

void DiagnosisServiceImpl::GetFaultRecord(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::GetFaultRecordRequest* request,
    deviceops::diagnosis::GetFaultRecordResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto fault = _repository->getFault(request->fault_id());
    if (!fault.has_value()) {
        setResponse(response->mutable_response(), 404, "fault not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_fault() = *fault;
}

void DiagnosisServiceImpl::ListFaultRecords(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::ListFaultRecordsRequest* request,
    deviceops::diagnosis::ListFaultRecordsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    FaultListFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.device_id = request->device_id();
    filter.event_id = request->event_id();
    filter.status = request->status();
    filter.start_time = request->time_range().start_time();
    filter.end_time = request->time_range().end_time();

    int64_t total = 0;
    const auto faults = _repository->listFaults(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& fault : faults) {
        *response->add_faults() = fault;
    }
}

void DiagnosisServiceImpl::StartDiagnosis(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::StartDiagnosisRequest* request,
    deviceops::diagnosis::StartDiagnosisResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    std::optional<deviceops::diagnosis::FaultRecord> fault;
    if (!request->fault_id().empty()) {
        fault = _repository->getFault(request->fault_id());
        if (!fault.has_value()) {
            setResponse(response->mutable_response(), 404, "fault not found");
            return;
        }
    }

    DiagnosisDraft draft;
    std::string error;
    if (!_rag_client->diagnose(*request, fault, &draft, &error)) {
        setResponse(response->mutable_response(), 503, error);
        return;
    }

    deviceops::diagnosis::DiagnosisReport report;
    if (!_repository->createDiagnosisReport(*request, draft, fault, &report, &error)) {
        setResponse(response->mutable_response(), 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    response->set_task_id(report.report_id());

    if (_event_publisher != nullptr && _event_publisher->enabled()) {
        Json::Value payload;
        payload["task_id"] = report.report_id();
        payload["report_id"] = report.report_id();
        payload["event_id"] = request->event_id();
        payload["fault_id"] = request->fault_id();
        payload["device_id"] = request->device_id();
        payload["requested_by"] = Json::UInt64(request->requested_by());
        payload["report_status"] = deviceops::common::ReportStatus_Name(report.status());
        payload["created_at"] = Json::Int64(report.created_at());

        std::string error;
        if (!_event_publisher->publishDiagnosisTaskCreated(payload, report.report_id(), &error)) {
            WRN("rabbitmq diagnosis.task.created publish failed: report_id={}, error={}",
                report.report_id(),
                error);
        }
    }
}

void DiagnosisServiceImpl::GetDiagnosisReport(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::GetDiagnosisReportRequest* request,
    deviceops::diagnosis::GetDiagnosisReportResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto report = _repository->getReport(request->report_id());
    if (!report.has_value()) {
        setResponse(response->mutable_response(), 404, "report not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_report() = *report;
}

void DiagnosisServiceImpl::ListDiagnosisReports(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::ListDiagnosisReportsRequest* request,
    deviceops::diagnosis::ListDiagnosisReportsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    ReportListFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.device_id = request->device_id();
    filter.event_id = request->event_id();
    filter.fault_id = request->fault_id();
    filter.status = request->status();

    int64_t total = 0;
    const auto reports = _repository->listReports(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& report : reports) {
        *response->add_reports() = report;
    }
}

void DiagnosisServiceImpl::ConfirmDiagnosisReport(::google::protobuf::RpcController* controller,
    const deviceops::diagnosis::ConfirmDiagnosisReportRequest* request,
    deviceops::diagnosis::ConfirmDiagnosisReportResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    deviceops::diagnosis::DiagnosisReport report;
    std::string error;
    if (!_repository->confirmReport(*request, &report, &error)) {
        setResponse(response->mutable_response(), error == "report not found" ? 404 : 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_report() = report;
}

} // namespace deviceops::diagnosis_service
