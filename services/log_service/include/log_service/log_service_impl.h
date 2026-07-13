#pragma once

#include "deviceops/mq/rabbitmq_event_publisher.h"
#include "log.pb.h"

namespace deviceops::log_service {

class LogRepository;

class LogServiceImpl final : public deviceops::log::LogService {
public:
    LogServiceImpl(LogRepository* repository, deviceops::mq::RabbitMqEventPublisher* event_publisher);

    void WriteLog(::google::protobuf::RpcController* controller,
        const deviceops::log::WriteLogRequest* request,
        deviceops::log::WriteLogResponse* response,
        ::google::protobuf::Closure* done) override;

    void QueryLogs(::google::protobuf::RpcController* controller,
        const deviceops::log::QueryLogsRequest* request,
        deviceops::log::QueryLogsResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetLogContext(::google::protobuf::RpcController* controller,
        const deviceops::log::GetLogContextRequest* request,
        deviceops::log::GetLogContextResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    LogRepository* _repository;
    deviceops::mq::RabbitMqEventPublisher* _event_publisher;
};

} // namespace deviceops::log_service
