#pragma once

#include "device_gateway.pb.h"

namespace deviceops::gateway {

class GatewayServer;

class GatewayRpcService final : public DeviceGatewayService {
public:
    explicit GatewayRpcService(GatewayServer* server);

    void GetGatewayStatus(::google::protobuf::RpcController* controller,
        const GetGatewayStatusRequest* request,
        GetGatewayStatusResponse* response,
        ::google::protobuf::Closure* done) override;

    void ListConnectedDevices(::google::protobuf::RpcController* controller,
        const ListConnectedDevicesRequest* request,
        ListConnectedDevicesResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetForwardingStats(::google::protobuf::RpcController* controller,
        const GetForwardingStatsRequest* request,
        GetForwardingStatsResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    GatewayServer* _server;
};

} // namespace deviceops::gateway
