#include "device_service/device_repository.h"

#include <algorithm>
#include <chrono>
#include <functional>

namespace deviceops::device_service {
namespace {

bool contains(const std::string& value, const std::string& keyword) {
    return keyword.empty() || value.find(keyword) != std::string::npos;
}

bool deviceMatchesKeyword(const deviceops::device::Device& device, const std::string& keyword) {
    return contains(device.device_id(), keyword)
        || contains(device.device_name(), keyword)
        || contains(device.model(), keyword)
        || contains(device.location(), keyword);
}

int normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return std::min(page_size, 100);
}

} // namespace

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool DeviceRepository::create(const deviceops::device::CreateDeviceRequest& request, DeviceRecord* created, std::string* error) {
    if (request.device_id().empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }
    if (request.device_name().empty()) {
        if (error != nullptr) {
            *error = "device_name is required";
        }
        return false;
    }
    if (request.device_type().empty()) {
        if (error != nullptr) {
            *error = "device_type is required";
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (_devices.find(request.device_id()) != _devices.end()) {
        if (error != nullptr) {
            *error = "device_id already exists";
        }
        return false;
    }

    const int64_t now = currentUnixMillis();
    DeviceRecord record;
    record.device.set_id(_next_id++);
    record.device.set_device_id(request.device_id());
    record.device.set_device_name(request.device_name());
    record.device.set_device_type(request.device_type());
    record.device.set_model(request.model());
    record.device.set_manufacturer(request.manufacturer());
    record.device.set_location(request.location());
    record.device.set_status(deviceops::common::DEVICE_STATUS_ENABLED);
    record.device.set_protocol(request.protocol().empty() ? "mqtt" : request.protocol());
    record.device.set_description(request.description());
    record.device.set_created_at(now);
    record.device.set_updated_at(now);
    record.access_token_hash = hashAccessToken(request.access_token());

    _devices.emplace(record.device.device_id(), record);
    if (created != nullptr) {
        *created = record;
    }
    return true;
}

bool DeviceRepository::update(const deviceops::device::UpdateDeviceRequest& request, DeviceRecord* updated, std::string* error) {
    if (request.device_id().empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _devices.find(request.device_id());
    if (it == _devices.end()) {
        if (error != nullptr) {
            *error = "device not found";
        }
        return false;
    }

    auto& device = it->second.device;
    if (!request.device_name().empty()) {
        device.set_device_name(request.device_name());
    }
    if (!request.device_type().empty()) {
        device.set_device_type(request.device_type());
    }
    if (!request.model().empty()) {
        device.set_model(request.model());
    }
    if (!request.manufacturer().empty()) {
        device.set_manufacturer(request.manufacturer());
    }
    if (!request.location().empty()) {
        device.set_location(request.location());
    }
    if (request.status() != deviceops::common::DEVICE_STATUS_UNSPECIFIED) {
        device.set_status(request.status());
    }
    if (!request.description().empty()) {
        device.set_description(request.description());
    }
    device.set_updated_at(currentUnixMillis());

    if (updated != nullptr) {
        *updated = it->second;
    }
    return true;
}

std::optional<DeviceRecord> DeviceRepository::get(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _devices.find(device_id);
    if (it == _devices.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<DeviceRecord> DeviceRepository::list(const ListDeviceFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<DeviceRecord> matched;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& item : _devices) {
            const auto& record = item.second;
            const auto& device = record.device;
            if (!filter.device_type.empty() && device.device_type() != filter.device_type) {
                continue;
            }
            if (filter.status != deviceops::common::DEVICE_STATUS_UNSPECIFIED && device.status() != filter.status) {
                continue;
            }
            if (!deviceMatchesKeyword(device, filter.keyword)) {
                continue;
            }
            matched.push_back(record);
        }
    }

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }

    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<DeviceRecord>(matched.begin() + begin, matched.begin() + end);
}

bool DeviceRepository::verifyAccess(const std::string& device_id, const std::string& access_token, const std::string& protocol, DeviceRecord* record, std::string* error) const {
    const auto found = get(device_id);
    if (!found.has_value()) {
        if (error != nullptr) {
            *error = "device not found";
        }
        return false;
    }

    const auto& device = found->device;
    if (device.status() != deviceops::common::DEVICE_STATUS_ENABLED) {
        if (error != nullptr) {
            *error = "device is not enabled";
        }
        return false;
    }
    if (!protocol.empty() && device.protocol() != protocol) {
        if (error != nullptr) {
            *error = "protocol mismatch";
        }
        return false;
    }
    if (found->access_token_hash != hashAccessToken(access_token)) {
        if (error != nullptr) {
            *error = "access token mismatch";
        }
        return false;
    }

    if (record != nullptr) {
        *record = *found;
    }
    return true;
}

std::string DeviceRepository::hashAccessToken(const std::string& access_token) {
    return std::to_string(std::hash<std::string>{}(access_token));
}

} // namespace deviceops::device_service
