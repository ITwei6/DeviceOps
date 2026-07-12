#pragma once

#include "telemetry.pb.h"
#include "telemetry_service/telemetry_repository.h"

namespace deviceops::telemetry_service {

class TelemetryServiceImpl final : public deviceops::telemetry::TelemetryService {
public:
    explicit TelemetryServiceImpl(TelemetryRepository* repository);

    void UploadTelemetry(::google::protobuf::RpcController* controller,
        const deviceops::telemetry::UploadTelemetryRequest* request,
        deviceops::telemetry::UploadTelemetryResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetRealtimeStatus(::google::protobuf::RpcController* controller,
        const deviceops::telemetry::GetRealtimeStatusRequest* request,
        deviceops::telemetry::GetRealtimeStatusResponse* response,
        ::google::protobuf::Closure* done) override;

    void ListRealtimeStatus(::google::protobuf::RpcController* controller,
        const deviceops::telemetry::ListRealtimeStatusRequest* request,
        deviceops::telemetry::ListRealtimeStatusResponse* response,
        ::google::protobuf::Closure* done) override;

    void QueryTelemetryHistory(::google::protobuf::RpcController* controller,
        const deviceops::telemetry::QueryTelemetryHistoryRequest* request,
        deviceops::telemetry::QueryTelemetryHistoryResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    TelemetryRepository* _repository;
};

} // namespace deviceops::telemetry_service
