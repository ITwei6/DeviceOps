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

void ensureSupplementalTables(const std::shared_ptr<odb::database>& db) {
    odb::transaction tx(db->begin());
    db->execute(
        "CREATE TABLE IF NOT EXISTS `knowledge_documents` ("
        "`id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,"
        "`document_id` VARCHAR(128) NOT NULL,"
        "`title` TEXT NOT NULL,"
        "`category` VARCHAR(128) NOT NULL,"
        "`device_type` VARCHAR(128) NULL,"
        "`error_code` VARCHAR(128) NULL,"
        "`content` LONGTEXT NOT NULL,"
        "`status` VARCHAR(32) NOT NULL,"
        "`created_by` BIGINT UNSIGNED NOT NULL,"
        "`created_at` BIGINT NOT NULL,"
        "`updated_at` BIGINT NOT NULL,"
        "UNIQUE INDEX `document_id_i` (`document_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    db->execute(
        "CREATE TABLE IF NOT EXISTS `fault_records` ("
        "`id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,"
        "`fault_id` VARCHAR(128) NOT NULL,"
        "`device_id` VARCHAR(128) NOT NULL,"
        "`event_id` VARCHAR(128) NULL,"
        "`owner_user_id` BIGINT UNSIGNED NOT NULL,"
        "`fault_type` VARCHAR(128) NOT NULL,"
        "`severity` VARCHAR(32) NOT NULL,"
        "`status` VARCHAR(32) NOT NULL,"
        "`symptom` LONGTEXT NOT NULL,"
        "`root_cause` LONGTEXT NULL,"
        "`solution` LONGTEXT NULL,"
        "`started_at` BIGINT NOT NULL,"
        "`resolved_at` BIGINT NULL,"
        "`created_at` BIGINT NOT NULL,"
        "`updated_at` BIGINT NOT NULL,"
        "UNIQUE INDEX `fault_id_i` (`fault_id`),"
        "INDEX `fault_device_time_i` (`device_id`, `started_at`),"
        "INDEX `fault_status_i` (`status`),"
        "INDEX `fault_event_i` (`event_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    db->execute(
        "CREATE TABLE IF NOT EXISTS `diagnosis_reports` ("
        "`id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,"
        "`report_id` VARCHAR(128) NOT NULL,"
        "`device_id` VARCHAR(128) NOT NULL,"
        "`event_id` VARCHAR(128) NULL,"
        "`fault_id` VARCHAR(128) NULL,"
        "`created_by` BIGINT UNSIGNED NOT NULL,"
        "`report_type` VARCHAR(32) NOT NULL,"
        "`status` VARCHAR(32) NOT NULL,"
        "`summary` LONGTEXT NOT NULL,"
        "`possible_causes_json` LONGTEXT NULL,"
        "`recommended_actions_json` LONGTEXT NULL,"
        "`evidence_json` LONGTEXT NULL,"
        "`ai_model` VARCHAR(128) NULL,"
        "`confidence` DOUBLE NOT NULL,"
        "`created_at` BIGINT NOT NULL,"
        "`updated_at` BIGINT NOT NULL,"
        "UNIQUE INDEX `report_id_i` (`report_id`),"
        "INDEX `report_device_time_i` (`device_id`, `created_at`),"
        "INDEX `report_event_i` (`event_id`),"
        "INDEX `report_fault_i` (`fault_id`),"
        "INDEX `report_status_i` (`status`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    tx.commit();
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
    ensureSupplementalTables(db);
}

} // namespace deviceops::db
