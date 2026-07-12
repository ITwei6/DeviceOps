#include "telemetry_service/telemetry_service_impl.h"

#include <brpc/server.h>

namespace deviceops::telemetry_service {
namespace {

void setResponse(deviceops::common::CommonResponse* response, int code, const std::string& message, const std::string& trace_id = "") {
    response->set_code(code);
    response->set_message(message);
    response->set_trace_id(trace_id);
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

TelemetryServiceImpl::TelemetryServiceImpl(TelemetryRepository* repository)
    : _repository(repository) {
}

void TelemetryServiceImpl::UploadTelemetry(::google::protobuf::RpcController* controller,
    const deviceops::telemetry::UploadTelemetryRequest* request,
    deviceops::telemetry::UploadTelemetryResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    if (request->telemetry().device_id().empty()) {
        setResponse(response->mutable_response(), 400, "device_id is required", request->trace_id());
        return;
    }

    _repository->upload(request->telemetry());
    setResponse(response->mutable_response(), 0, "ok", request->trace_id());
}

void TelemetryServiceImpl::GetRealtimeStatus(::google::protobuf::RpcController* controller,
    const deviceops::telemetry::GetRealtimeStatusRequest* request,
    deviceops::telemetry::GetRealtimeStatusResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto telemetry = _repository->getRealtime(request->device_id());
    if (!telemetry.has_value()) {
        setResponse(response->mutable_response(), 404, "telemetry not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_telemetry() = *telemetry;
}

void TelemetryServiceImpl::ListRealtimeStatus(::google::protobuf::RpcController* controller,
    const deviceops::telemetry::ListRealtimeStatusRequest* request,
    deviceops::telemetry::ListRealtimeStatusResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    ListTelemetryFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.only_online = request->only_online();
    filter.device_ids.assign(request->device_ids().begin(), request->device_ids().end());

    int64_t total = 0;
    const auto items = _repository->listRealtime(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& item : items) {
        *response->add_items() = item;
    }
}

void TelemetryServiceImpl::QueryTelemetryHistory(::google::protobuf::RpcController* controller,
    const deviceops::telemetry::QueryTelemetryHistoryRequest* request,
    deviceops::telemetry::QueryTelemetryHistoryResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    QueryHistoryFilter filter;
    filter.device_id = request->device_id();
    filter.start_time = request->time_range().start_time();
    filter.end_time = request->time_range().end_time();
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());

    int64_t total = 0;
    const auto items = _repository->queryHistory(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& item : items) {
        *response->add_items() = item;
    }
}

} // namespace deviceops::telemetry_service
