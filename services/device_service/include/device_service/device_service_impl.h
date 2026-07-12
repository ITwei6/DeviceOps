#pragma once

#include "device.pb.h"
#include "device_service/device_repository.h"

namespace deviceops::device_service {

class DeviceServiceImpl final : public deviceops::device::DeviceService {
public:
    explicit DeviceServiceImpl(DeviceRepository* repository);

    void CreateDevice(::google::protobuf::RpcController* controller,
        const deviceops::device::CreateDeviceRequest* request,
        deviceops::device::CreateDeviceResponse* response,
        ::google::protobuf::Closure* done) override;

    void UpdateDevice(::google::protobuf::RpcController* controller,
        const deviceops::device::UpdateDeviceRequest* request,
        deviceops::device::UpdateDeviceResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetDevice(::google::protobuf::RpcController* controller,
        const deviceops::device::GetDeviceRequest* request,
        deviceops::device::GetDeviceResponse* response,
        ::google::protobuf::Closure* done) override;

    void ListDevices(::google::protobuf::RpcController* controller,
        const deviceops::device::ListDevicesRequest* request,
        deviceops::device::ListDevicesResponse* response,
        ::google::protobuf::Closure* done) override;

    void VerifyDeviceAccess(::google::protobuf::RpcController* controller,
        const deviceops::device::VerifyDeviceAccessRequest* request,
        deviceops::device::VerifyDeviceAccessResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    DeviceRepository* _repository;
};

} // namespace deviceops::device_service
