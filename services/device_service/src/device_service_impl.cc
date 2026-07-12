#include "device_service/device_service_impl.h"

#include <brpc/server.h>

namespace deviceops::device_service {
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

DeviceServiceImpl::DeviceServiceImpl(DeviceRepository* repository)
    : _repository(repository) {
}

void DeviceServiceImpl::CreateDevice(::google::protobuf::RpcController* controller,
    const deviceops::device::CreateDeviceRequest* request,
    deviceops::device::CreateDeviceResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    DeviceRecord record;
    std::string error;
    if (!_repository->create(*request, &record, &error)) {
        setResponse(response->mutable_response(), error == "device_id already exists" ? 409 : 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_device() = record.device;
}

void DeviceServiceImpl::UpdateDevice(::google::protobuf::RpcController* controller,
    const deviceops::device::UpdateDeviceRequest* request,
    deviceops::device::UpdateDeviceResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    DeviceRecord record;
    std::string error;
    if (!_repository->update(*request, &record, &error)) {
        setResponse(response->mutable_response(), error == "device not found" ? 404 : 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_device() = record.device;
}

void DeviceServiceImpl::GetDevice(::google::protobuf::RpcController* controller,
    const deviceops::device::GetDeviceRequest* request,
    deviceops::device::GetDeviceResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto record = _repository->get(request->device_id());
    if (!record.has_value()) {
        setResponse(response->mutable_response(), 404, "device not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_device() = record->device;
}

void DeviceServiceImpl::ListDevices(::google::protobuf::RpcController* controller,
    const deviceops::device::ListDevicesRequest* request,
    deviceops::device::ListDevicesResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    ListDeviceFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.device_type = request->device_type();
    filter.status = request->status();
    filter.keyword = request->keyword();

    int64_t total = 0;
    const auto devices = _repository->list(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& record : devices) {
        *response->add_devices() = record.device;
    }
}

void DeviceServiceImpl::VerifyDeviceAccess(::google::protobuf::RpcController* controller,
    const deviceops::device::VerifyDeviceAccessRequest* request,
    deviceops::device::VerifyDeviceAccessResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    DeviceRecord record;
    std::string error;
    if (!_repository->verifyAccess(request->device_id(), request->access_token(), request->protocol(), &record, &error)) {
        setResponse(response->mutable_response(), 403, error);
        response->set_allowed(false);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    response->set_allowed(true);
    *response->mutable_device() = record.device;
}

} // namespace deviceops::device_service
