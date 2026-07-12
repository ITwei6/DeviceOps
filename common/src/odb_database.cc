#include "deviceops/db/odb_database.h"

#include <chrono>
#include <cstdlib>
#include <stdexcept>

#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>

#include "odb.h"

namespace deviceops::db {
namespace {

std::string getenvOrDefault(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

int getenvIntOrDefault(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::string quoteIdentifier(const std::string& identifier) {
    if (identifier.empty()) {
        throw std::invalid_argument("empty MySQL identifier");
    }
    for (char ch : identifier) {
        const bool valid = (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '_';
        if (!valid) {
            throw std::invalid_argument("invalid MySQL identifier: " + identifier);
        }
    }
    return "`" + identifier + "`";
}

std::shared_ptr<odb::database> connectWithDatabase(const OdbConfig& config, const std::string& database) {
    tewodb::mysql_settings settings;
    settings.host = config.host;
    settings.port = static_cast<unsigned int>(config.port);
    settings.user = config.user;
    settings.passwd = config.password;
    settings.db = database;
    settings.cset = "utf8mb4";
    return tewodb::DBFactory::mysql(settings);
}

} // namespace

int64_t currentUnixMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

OdbConfig loadOdbConfigFromEnv() {
    OdbConfig config;
    config.host = getenvOrDefault("DEVICEOPS_MYSQL_HOST", config.host);
    config.port = getenvIntOrDefault("DEVICEOPS_MYSQL_PORT", config.port);
    config.user = getenvOrDefault("DEVICEOPS_MYSQL_USER", config.user);
    config.password = getenvOrDefault("DEVICEOPS_MYSQL_PASSWORD", config.password);
    config.database = getenvOrDefault("DEVICEOPS_MYSQL_DATABASE", config.database);
    return config;
}

std::shared_ptr<odb::database> createOdbDatabase(const OdbConfig& config) {
    return connectWithDatabase(config, config.database);
}

void ensureOdbSchema(const OdbConfig& config) {
    auto bootstrap = connectWithDatabase(config, "mysql");
    odb::transaction bootstrap_tx(bootstrap->begin());
    bootstrap->execute("CREATE DATABASE IF NOT EXISTS " + quoteIdentifier(config.database)
        + " DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
    bootstrap_tx.commit();

    auto db = createOdbDatabase(config);
    try {
        odb::transaction schema_tx(db->begin());
        odb::schema_catalog::create_schema(*db);
        schema_tx.commit();
    } catch (const odb::exception&) {
        // Schema creation is best-effort on service startup. Existing tables are
        // expected after the first boot because generated ODB DDL is not idempotent.
    }
}

} // namespace deviceops::db
