#pragma once

#include "deviceops/mq/rabbitmq_event_publisher.h"
#include "event.pb.h"
#include "event_service/event_repository.h"

namespace deviceops::event_service {

class EventServiceImpl final : public deviceops::event::EventService {
public:
    EventServiceImpl(EventRepository* repository, deviceops::mq::RabbitMqEventPublisher* event_publisher);

    void CreateEvent(::google::protobuf::RpcController* controller,
        const deviceops::event::CreateEventRequest* request,
        deviceops::event::CreateEventResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetEvent(::google::protobuf::RpcController* controller,
        const deviceops::event::GetEventRequest* request,
        deviceops::event::GetEventResponse* response,
        ::google::protobuf::Closure* done) override;

    void ListEvents(::google::protobuf::RpcController* controller,
        const deviceops::event::ListEventsRequest* request,
        deviceops::event::ListEventsResponse* response,
        ::google::protobuf::Closure* done) override;

    void UpdateEventStatus(::google::protobuf::RpcController* controller,
        const deviceops::event::UpdateEventStatusRequest* request,
        deviceops::event::UpdateEventStatusResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    EventRepository* _repository;
    deviceops::mq::RabbitMqEventPublisher* _event_publisher;
};

} // namespace deviceops::event_service
