#include "log_service/log_service_impl.h"

#include <brpc/server.h>

#include "log_service/log_repository.h"

namespace deviceops::log_service {
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

LogServiceImpl::LogServiceImpl(LogRepository* repository)
    : _repository(repository) {
}

void LogServiceImpl::WriteLog(::google::protobuf::RpcController* controller,
    const deviceops::log::WriteLogRequest* request,
    deviceops::log::WriteLogResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    std::string error;
    if (!_repository->write(request->log(), &error)) {
        setResponse(response->mutable_response(), 503, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
}

void LogServiceImpl::QueryLogs(::google::protobuf::RpcController* controller,
    const deviceops::log::QueryLogsRequest* request,
    deviceops::log::QueryLogsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    LogQueryFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.device_id = request->device_id();
    filter.service_name = request->service_name();
    filter.level = request->level();
    filter.keyword = request->keyword();
    filter.event_id = request->event_id();
    filter.start_time = request->time_range().start_time();
    filter.end_time = request->time_range().end_time();

    int64_t total = 0;
    const auto logs = _repository->query(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& log : logs) {
        *response->add_logs() = log;
    }
}

void LogServiceImpl::GetLogContext(::google::protobuf::RpcController* controller,
    const deviceops::log::GetLogContextRequest* request,
    deviceops::log::GetLogContextResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    if (request->device_id().empty()) {
        setResponse(response->mutable_response(), 400, "device_id is required");
        return;
    }
    if (request->center_time() <= 0) {
        setResponse(response->mutable_response(), 400, "center_time is required");
        return;
    }

    const auto logs = _repository->context(
        request->device_id(),
        request->center_time(),
        request->before_ms(),
        request->after_ms(),
        request->limit());

    setResponse(response->mutable_response(), 0, "ok");
    for (const auto& log : logs) {
        *response->add_logs() = log;
    }
}

} // namespace deviceops::log_service
