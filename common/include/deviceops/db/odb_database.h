#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

namespace deviceops::db {

struct OdbConfig {
    std::string host = "mysql-service";
    int port = 3306;
    std::string user = "root";
    std::string password = "123456";
    std::string database = "deviceops";
};

OdbConfig loadOdbConfigFromEnv();
std::shared_ptr<odb::database> createOdbDatabase(const OdbConfig& config);
void ensureOdbSchema(const OdbConfig& config);
int64_t currentUnixMillis();

} // namespace deviceops::db
