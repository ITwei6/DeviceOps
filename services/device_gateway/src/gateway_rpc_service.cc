#include "device_gateway/gateway_rpc_service.h"

#include <brpc/server.h>

#include "device_gateway/gateway_server.h"

namespace deviceops::gateway {
namespace {

void setResponse(deviceops::common::CommonResponse* response, int code, const std::string& message) {
    response->set_code(code);
    response->set_message(message);
}

bool gatewayMatches(const std::string& requested, const GatewayServer* server) {
    return requested.empty() || requested == server->gatewayId();
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

GatewayRpcService::GatewayRpcService(GatewayServer* server)
    : _server(server) {
}

void GatewayRpcService::GetGatewayStatus(::google::protobuf::RpcController* controller,
    const GetGatewayStatusRequest* request,
    GetGatewayStatusResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);
    if (!gatewayMatches(request->gateway_id(), _server)) {
        setResponse(response->mutable_response(), 404, "gateway not found");
        return;
    }
    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_status() = _server->currentStatus();
}

void GatewayRpcService::ListConnectedDevices(::google::protobuf::RpcController* controller,
    const ListConnectedDevicesRequest* request,
    ListConnectedDevicesResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);
    if (!gatewayMatches(request->gateway_id(), _server)) {
        setResponse(response->mutable_response(), 404, "gateway not found");
        return;
    }

    const int page = pageOrDefault(request->page());
    const int page_size = pageSizeOrDefault(request->page());
    int64_t total = 0;
    const auto devices = _server->connectedDevices(request->keyword(), page, page_size, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(page);
    response->mutable_page()->set_page_size(page_size);
    response->mutable_page()->set_total(total);
    for (const auto& device : devices) {
        *response->add_devices() = device;
    }
}

void GatewayRpcService::GetForwardingStats(::google::protobuf::RpcController* controller,
    const GetForwardingStatsRequest* request,
    GetForwardingStatsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);
    if (!gatewayMatches(request->gateway_id(), _server)) {
        setResponse(response->mutable_response(), 404, "gateway not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    const auto stats = _server->forwardingStats(request->message_type());
    for (const auto& stat : stats) {
        *response->add_stats() = stat;
    }
}

} // namespace deviceops::gateway
