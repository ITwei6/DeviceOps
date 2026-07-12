#include "device_service/device_repository.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <utility>

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "device_entity-odb.hxx"
#include "deviceops/db/odb_database.h"

namespace deviceops::device_service {
namespace {

bool contains(const std::string& value, const std::string& keyword) {
    return keyword.empty() || value.find(keyword) != std::string::npos;
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

std::string nullableString(const odb::nullable<std::string>& value) {
    return value ? value.get() : "";
}

} // namespace

DeviceRepository::DeviceRepository(std::shared_ptr<odb::database> database)
    : _database(std::move(database)) {
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

    try {
        const int64_t now = deviceops::db::currentUnixMillis();
        deviceops::db::DeviceEntity entity;
        entity.device_id = request.device_id();
        entity.device_name = request.device_name();
        entity.device_type = request.device_type();
        if (!request.model().empty()) {
            entity.model = request.model();
        }
        if (!request.manufacturer().empty()) {
            entity.manufacturer = request.manufacturer();
        }
        if (!request.location().empty()) {
            entity.location = request.location();
        }
        entity.status = "enabled";
        entity.access_token_hash = hashAccessToken(request.access_token());
        entity.protocol = request.protocol().empty() ? "mqtt" : request.protocol();
        if (!request.description().empty()) {
            entity.description = request.description();
        }
        entity.created_at = now;
        entity.updated_at = now;

        using Query = odb::query<deviceops::db::DeviceEntity>;
        odb::transaction tx(_database->begin());
        auto existing = _database->query_one<deviceops::db::DeviceEntity>(Query::device_id == request.device_id());
        if (existing) {
            tx.commit();
            if (error != nullptr) {
                *error = "device_id already exists";
            }
            return false;
        }
        _database->persist(entity);
        tx.commit();

        if (created != nullptr) {
            created->device = toProto(entity);
            created->access_token_hash = nullableString(entity.access_token_hash);
        }
        return true;
    } catch (const odb::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

bool DeviceRepository::update(const deviceops::device::UpdateDeviceRequest& request, DeviceRecord* updated, std::string* error) {
    if (request.device_id().empty()) {
        if (error != nullptr) {
            *error = "device_id is required";
        }
        return false;
    }

    try {
        using Query = odb::query<deviceops::db::DeviceEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::DeviceEntity>(Query::device_id == request.device_id());
        if (!entity) {
            tx.commit();
            if (error != nullptr) {
                *error = "device not found";
            }
            return false;
        }

        if (!request.device_name().empty()) {
            entity->device_name = request.device_name();
        }
        if (!request.device_type().empty()) {
            entity->device_type = request.device_type();
        }
        if (!request.model().empty()) {
            entity->model = request.model();
        }
        if (!request.manufacturer().empty()) {
            entity->manufacturer = request.manufacturer();
        }
        if (!request.location().empty()) {
            entity->location = request.location();
        }
        if (request.status() != deviceops::common::DEVICE_STATUS_UNSPECIFIED) {
            entity->status = statusToString(request.status());
        }
        if (!request.description().empty()) {
            entity->description = request.description();
        }
        entity->updated_at = deviceops::db::currentUnixMillis();
        _database->update(*entity);
        tx.commit();

        if (updated != nullptr) {
            updated->device = toProto(*entity);
            updated->access_token_hash = nullableString(entity->access_token_hash);
        }
        return true;
    } catch (const odb::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

std::optional<DeviceRecord> DeviceRepository::get(const std::string& device_id) const {
    try {
        using Query = odb::query<deviceops::db::DeviceEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::DeviceEntity>(Query::device_id == device_id);
        tx.commit();
        if (!entity) {
            return std::nullopt;
        }
        DeviceRecord record;
        record.device = toProto(*entity);
        record.access_token_hash = nullableString(entity->access_token_hash);
        return record;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<DeviceRecord> DeviceRepository::list(const ListDeviceFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<DeviceRecord> matched;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::DeviceEntity>;
        Result result(_database->query<deviceops::db::DeviceEntity>());
        for (const auto& entity : result) {
            const auto proto = toProto(entity);
            if (!filter.device_type.empty() && proto.device_type() != filter.device_type) {
                continue;
            }
            if (filter.status != deviceops::common::DEVICE_STATUS_UNSPECIFIED && proto.status() != filter.status) {
                continue;
            }
            if (!filter.keyword.empty()
                && !contains(proto.device_id(), filter.keyword)
                && !contains(proto.device_name(), filter.keyword)
                && !contains(proto.model(), filter.keyword)
                && !contains(proto.location(), filter.keyword)) {
                continue;
            }
            matched.push_back(DeviceRecord{proto, nullableString(entity.access_token_hash)});
        }
        tx.commit();
    } catch (const odb::exception&) {
        matched.clear();
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
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : access_token) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return std::to_string(hash);
}

deviceops::device::Device DeviceRepository::toProto(const deviceops::db::DeviceEntity& entity) {
    deviceops::device::Device device;
    device.set_id(entity.id);
    device.set_device_id(entity.device_id);
    device.set_device_name(entity.device_name);
    device.set_device_type(entity.device_type);
    device.set_model(nullableString(entity.model));
    device.set_manufacturer(nullableString(entity.manufacturer));
    device.set_location(nullableString(entity.location));
    device.set_status(statusFromString(entity.status));
    device.set_protocol(entity.protocol);
    device.set_description(nullableString(entity.description));
    device.set_created_at(entity.created_at);
    device.set_updated_at(entity.updated_at);
    return device;
}

deviceops::common::DeviceStatus DeviceRepository::statusFromString(const std::string& status) {
    if (status == "enabled") {
        return deviceops::common::DEVICE_STATUS_ENABLED;
    }
    if (status == "disabled") {
        return deviceops::common::DEVICE_STATUS_DISABLED;
    }
    if (status == "maintenance") {
        return deviceops::common::DEVICE_STATUS_MAINTENANCE;
    }
    return deviceops::common::DEVICE_STATUS_UNSPECIFIED;
}

std::string DeviceRepository::statusToString(deviceops::common::DeviceStatus status) {
    switch (status) {
    case deviceops::common::DEVICE_STATUS_ENABLED:
        return "enabled";
    case deviceops::common::DEVICE_STATUS_DISABLED:
        return "disabled";
    case deviceops::common::DEVICE_STATUS_MAINTENANCE:
        return "maintenance";
    default:
        return "unspecified";
    }
}

} // namespace deviceops::device_service
