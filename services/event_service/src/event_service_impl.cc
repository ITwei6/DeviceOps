#include "event_service/event_service_impl.h"

#include <brpc/server.h>
#include <jsoncpp/json/json.h>

#include "log.h"

namespace deviceops::event_service {
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

EventServiceImpl::EventServiceImpl(EventRepository* repository, deviceops::mq::RabbitMqEventPublisher* event_publisher)
    : _repository(repository),
      _event_publisher(event_publisher) {
}

void EventServiceImpl::CreateEvent(::google::protobuf::RpcController* controller,
    const deviceops::event::CreateEventRequest* request,
    deviceops::event::CreateEventResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    deviceops::event::Event event;
    std::string error;
    if (!_repository->create(*request, &event, &error)) {
        setResponse(response->mutable_response(), 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_event() = event;

    if (_event_publisher != nullptr && _event_publisher->enabled()) {
        Json::Value payload;
        payload["event_id"] = event.event_id();
        payload["device_id"] = event.device_id();
        payload["event_type"] = event.event_type();
        payload["severity"] = deviceops::common::EventSeverity_Name(event.severity());
        payload["status"] = deviceops::common::EventStatus_Name(event.status());
        payload["error_code"] = event.error_code();
        payload["title"] = event.title();
        payload["description"] = event.description();
        payload["rule_name"] = event.rule_name();
        payload["occurred_at"] = Json::Int64(event.occurred_at());
        payload["created_at"] = Json::Int64(event.created_at());

        std::string error;
        if (!_event_publisher->publishAlarmCreated(payload, request->trace_id(), &error)) {
            WRN("rabbitmq event.alarm.created publish failed: event_id={}, error={}", event.event_id(), error);
        }
    }
}

void EventServiceImpl::GetEvent(::google::protobuf::RpcController* controller,
    const deviceops::event::GetEventRequest* request,
    deviceops::event::GetEventResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto event = _repository->get(request->event_id());
    if (!event.has_value()) {
        setResponse(response->mutable_response(), 404, "event not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_event() = *event;
}

void EventServiceImpl::ListEvents(::google::protobuf::RpcController* controller,
    const deviceops::event::ListEventsRequest* request,
    deviceops::event::ListEventsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    ListEventFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.device_id = request->device_id();
    filter.event_type = request->event_type();
    filter.severity = request->severity();
    filter.status = request->status();
    filter.start_time = request->time_range().start_time();
    filter.end_time = request->time_range().end_time();

    int64_t total = 0;
    const auto events = _repository->list(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& event : events) {
        *response->add_events() = event;
    }
}

void EventServiceImpl::UpdateEventStatus(::google::protobuf::RpcController* controller,
    const deviceops::event::UpdateEventStatusRequest* request,
    deviceops::event::UpdateEventStatusResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    deviceops::event::Event event;
    std::string error;
    if (!_repository->updateStatus(request->event_id(), request->status(), &event, &error)) {
        setResponse(response->mutable_response(), error == "event not found" ? 404 : 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_event() = event;
}

} // namespace deviceops::event_service
