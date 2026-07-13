#pragma once

#include <cstdint>
#include <string>

#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace deviceops::db {

#pragma db object table("knowledge_documents")
struct KnowledgeEntity {
    #pragma db id auto column("id")
    uint64_t id = 0;

    #pragma db unique column("document_id") type("VARCHAR(128)")
    std::string document_id;

    #pragma db column("title")
    std::string title;

    #pragma db column("category") type("VARCHAR(128)")
    std::string category;

    #pragma db column("device_type") type("VARCHAR(128)") null
    odb::nullable<std::string> device_type;

    #pragma db column("error_code") type("VARCHAR(128)") null
    odb::nullable<std::string> error_code;

    #pragma db column("content") type("LONGTEXT")
    std::string content;

    #pragma db column("status") type("VARCHAR(32)")
    std::string status = "active";

    #pragma db column("created_by")
    uint64_t created_by = 0;

    #pragma db column("created_at")
    int64_t created_at = 0;

    #pragma db column("updated_at")
    int64_t updated_at = 0;
};

} // namespace deviceops::db
