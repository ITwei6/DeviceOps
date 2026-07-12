#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "device.pb.h"

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
    bool create(const deviceops::device::CreateDeviceRequest& request, DeviceRecord* created, std::string* error);
    bool update(const deviceops::device::UpdateDeviceRequest& request, DeviceRecord* updated, std::string* error);
    std::optional<DeviceRecord> get(const std::string& device_id) const;
    std::vector<DeviceRecord> list(const ListDeviceFilter& filter, int64_t* total) const;
    bool verifyAccess(const std::string& device_id, const std::string& access_token, const std::string& protocol, DeviceRecord* record, std::string* error) const;

private:
    static std::string hashAccessToken(const std::string& access_token);

private:
    mutable std::mutex _mutex;
    uint64_t _next_id = 1;
    std::map<std::string, DeviceRecord> _devices;
};

int64_t currentUnixMillis();

} // namespace deviceops::device_service
