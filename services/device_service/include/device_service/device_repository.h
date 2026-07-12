#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "device.pb.h"
#include "deviceops/db/device_entity.h"

namespace deviceops::device_service {

struct DeviceRecord {
    deviceops::device::Device device;
    std::string access_token_hash;
};

struct ListDeviceFilter {
    int page = 1;
    int page_size = 20;
    std::string device_type;
    deviceops::common::DeviceStatus status = deviceops::common::DEVICE_STATUS_UNSPECIFIED;
    std::string keyword;
};

class DeviceRepository {
public:
    explicit DeviceRepository(std::shared_ptr<odb::database> database);

    bool create(const deviceops::device::CreateDeviceRequest& request, DeviceRecord* created, std::string* error);
    bool update(const deviceops::device::UpdateDeviceRequest& request, DeviceRecord* updated, std::string* error);
    std::optional<DeviceRecord> get(const std::string& device_id) const;
    std::vector<DeviceRecord> list(const ListDeviceFilter& filter, int64_t* total) const;
    bool verifyAccess(const std::string& device_id, const std::string& access_token, const std::string& protocol, DeviceRecord* record, std::string* error) const;

private:
    static deviceops::device::Device toProto(const deviceops::db::DeviceEntity& entity);
    static deviceops::common::DeviceStatus statusFromString(const std::string& status);
    static std::string statusToString(deviceops::common::DeviceStatus status);
    static std::string hashAccessToken(const std::string& access_token);

private:
    std::shared_ptr<odb::database> _database;
};

} // namespace deviceops::device_service
