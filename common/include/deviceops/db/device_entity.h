#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace deviceops::db {

#pragma db object table("devices")
struct DeviceEntity {
    #pragma db id auto column("id")
    uint64_t id = 0;

    #pragma db unique column("device_id") type("VARCHAR(128)")
    std::string device_id;

    #pragma db column("device_name")
    std::string device_name;

    #pragma db column("device_type")
    std::string device_type;

    #pragma db column("model") null
    odb::nullable<std::string> model;

    #pragma db column("manufacturer") null
    odb::nullable<std::string> manufacturer;

    #pragma db column("location") null
    odb::nullable<std::string> location;

    #pragma db column("status")
    std::string status = "enabled";

    #pragma db column("access_token_hash") null
    odb::nullable<std::string> access_token_hash;

    #pragma db column("protocol")
    std::string protocol = "mqtt";

    #pragma db column("description") null
    odb::nullable<std::string> description;

    #pragma db column("created_at")
    int64_t created_at = 0;

    #pragma db column("updated_at")
    int64_t updated_at = 0;
};

} // namespace deviceops::db
