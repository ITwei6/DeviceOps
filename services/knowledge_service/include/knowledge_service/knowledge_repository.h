#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "deviceops/db/knowledge_entity.h"
#include "knowledge.pb.h"

namespace deviceops::knowledge_service {

struct KnowledgeListFilter {
    int page = 1;
    int page_size = 20;
    std::string category;
    std::string device_type;
    std::string keyword;
    std::string status;
};

class KnowledgeRepository {
public:
    explicit KnowledgeRepository(std::shared_ptr<odb::database> database);

    bool create(const deviceops::knowledge::CreateKnowledgeDocumentRequest& request, deviceops::knowledge::KnowledgeDocument* created, std::string* error);
    std::optional<deviceops::knowledge::KnowledgeDocument> get(const std::string& document_id) const;
    std::vector<deviceops::knowledge::KnowledgeDocument> list(const KnowledgeListFilter& filter, int64_t* total) const;
    std::vector<deviceops::knowledge::KnowledgeSnippet> search(const deviceops::knowledge::SearchKnowledgeRequest& request) const;

private:
    std::string nextDocumentId(int64_t now);
    static deviceops::knowledge::KnowledgeDocument toProto(const deviceops::db::KnowledgeEntity& entity);
    static deviceops::knowledge::KnowledgeSnippet toSnippet(const deviceops::db::KnowledgeEntity& entity, const std::string& query, double score);
    static bool matchesListFilter(const deviceops::knowledge::KnowledgeDocument& document, const KnowledgeListFilter& filter);
    static bool matchesSearchFilter(const deviceops::db::KnowledgeEntity& entity, const deviceops::knowledge::SearchKnowledgeRequest& request);
    static double scoreDocument(const deviceops::db::KnowledgeEntity& entity, const std::string& query);
    static std::string nullableString(const odb::nullable<std::string>& value);
    static bool contains(const std::string& value, const std::string& keyword);
    static int normalizePage(int page);
    static int normalizePageSize(int page_size);

private:
    std::shared_ptr<odb::database> _database;
    std::atomic<uint64_t> _sequence{1};
};

} // namespace deviceops::knowledge_service
