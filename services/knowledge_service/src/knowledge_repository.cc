#include "knowledge_service/knowledge_repository.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "deviceops/db/odb_database.h"
#include "knowledge_entity-odb.hxx"

namespace deviceops::knowledge_service {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string firstChunk(const std::string& content) {
    constexpr size_t kMaxSnippetLength = 360;
    if (content.size() <= kMaxSnippetLength) {
        return content;
    }
    return content.substr(0, kMaxSnippetLength);
}

} // namespace

KnowledgeRepository::KnowledgeRepository(std::shared_ptr<odb::database> database)
    : _database(std::move(database)) {
}

bool KnowledgeRepository::create(const deviceops::knowledge::CreateKnowledgeDocumentRequest& request, deviceops::knowledge::KnowledgeDocument* created, std::string* error) {
    if (request.title().empty()) {
        if (error != nullptr) {
            *error = "title is required";
        }
        return false;
    }
    if (request.category().empty()) {
        if (error != nullptr) {
            *error = "category is required";
        }
        return false;
    }
    if (request.content().empty()) {
        if (error != nullptr) {
            *error = "content is required";
        }
        return false;
    }

    try {
        const int64_t now = deviceops::db::currentUnixMillis();
        deviceops::db::KnowledgeEntity entity;
        entity.document_id = nextDocumentId(now);
        entity.title = request.title();
        entity.category = request.category();
        if (!request.device_type().empty()) {
            entity.device_type = request.device_type();
        }
        if (!request.error_code().empty()) {
            entity.error_code = request.error_code();
        }
        entity.content = request.content();
        entity.status = "active";
        entity.created_by = request.created_by();
        entity.created_at = now;
        entity.updated_at = now;

        odb::transaction tx(_database->begin());
        _database->persist(entity);
        tx.commit();

        if (created != nullptr) {
            *created = toProto(entity);
        }
        return true;
    } catch (const odb::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

std::optional<deviceops::knowledge::KnowledgeDocument> KnowledgeRepository::get(const std::string& document_id) const {
    try {
        using Query = odb::query<deviceops::db::KnowledgeEntity>;
        odb::transaction tx(_database->begin());
        auto entity = _database->query_one<deviceops::db::KnowledgeEntity>(Query::document_id == document_id);
        tx.commit();
        if (!entity) {
            return std::nullopt;
        }
        return toProto(*entity);
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<deviceops::knowledge::KnowledgeDocument> KnowledgeRepository::list(const KnowledgeListFilter& filter, int64_t* total) const {
    const int page = normalizePage(filter.page);
    const int page_size = normalizePageSize(filter.page_size);

    std::vector<deviceops::knowledge::KnowledgeDocument> matched;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::KnowledgeEntity>;
        Result result(_database->query<deviceops::db::KnowledgeEntity>());
        for (const auto& entity : result) {
            auto document = toProto(entity);
            if (matchesListFilter(document, filter)) {
                matched.push_back(std::move(document));
            }
        }
        tx.commit();
    } catch (const odb::exception&) {
        matched.clear();
    }

    std::sort(matched.begin(), matched.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.updated_at() > rhs.updated_at();
    });

    if (total != nullptr) {
        *total = static_cast<int64_t>(matched.size());
    }

    const size_t begin = static_cast<size_t>((page - 1) * page_size);
    if (begin >= matched.size()) {
        return {};
    }
    const size_t end = std::min(matched.size(), begin + static_cast<size_t>(page_size));
    return std::vector<deviceops::knowledge::KnowledgeDocument>(matched.begin() + begin, matched.begin() + end);
}

std::vector<deviceops::knowledge::KnowledgeSnippet> KnowledgeRepository::search(const deviceops::knowledge::SearchKnowledgeRequest& request) const {
    std::vector<deviceops::knowledge::KnowledgeSnippet> snippets;
    try {
        odb::transaction tx(_database->begin());
        using Result = odb::result<deviceops::db::KnowledgeEntity>;
        Result result(_database->query<deviceops::db::KnowledgeEntity>());
        for (const auto& entity : result) {
            if (!matchesSearchFilter(entity, request)) {
                continue;
            }
            const double score = scoreDocument(entity, request.query());
            if (!request.query().empty() && score <= 0.0) {
                continue;
            }
            snippets.push_back(toSnippet(entity, request.query(), score));
        }
        tx.commit();
    } catch (const odb::exception&) {
        snippets.clear();
    }

    std::sort(snippets.begin(), snippets.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.score() > rhs.score();
    });

    const int top_k = request.top_k() <= 0 ? 5 : std::min(request.top_k(), 50);
    if (snippets.size() > static_cast<size_t>(top_k)) {
        snippets.resize(static_cast<size_t>(top_k));
    }
    return snippets;
}

std::string KnowledgeRepository::nextDocumentId(int64_t now) {
    std::ostringstream oss;
    oss << "doc-" << now << "-" << _sequence.fetch_add(1);
    return oss.str();
}

deviceops::knowledge::KnowledgeDocument KnowledgeRepository::toProto(const deviceops::db::KnowledgeEntity& entity) {
    deviceops::knowledge::KnowledgeDocument document;
    document.set_document_id(entity.document_id);
    document.set_title(entity.title);
    document.set_category(entity.category);
    document.set_device_type(nullableString(entity.device_type));
    document.set_error_code(nullableString(entity.error_code));
    document.set_content(entity.content);
    document.set_status(entity.status);
    document.set_created_at(entity.created_at);
    document.set_updated_at(entity.updated_at);
    return document;
}

deviceops::knowledge::KnowledgeSnippet KnowledgeRepository::toSnippet(const deviceops::db::KnowledgeEntity& entity, const std::string& query, double score) {
    deviceops::knowledge::KnowledgeSnippet snippet;
    snippet.set_document_id(entity.document_id);
    snippet.set_chunk_id(entity.document_id + "#chunk-0");
    snippet.set_title(entity.title);
    snippet.set_content(firstChunk(entity.content));
    snippet.set_score(score);
    (*snippet.mutable_metadata())["category"] = entity.category;
    (*snippet.mutable_metadata())["device_type"] = nullableString(entity.device_type);
    (*snippet.mutable_metadata())["error_code"] = nullableString(entity.error_code);
    (*snippet.mutable_metadata())["query"] = query;
    return snippet;
}

bool KnowledgeRepository::matchesListFilter(const deviceops::knowledge::KnowledgeDocument& document, const KnowledgeListFilter& filter) {
    if (!filter.category.empty() && document.category() != filter.category) {
        return false;
    }
    if (!filter.device_type.empty() && document.device_type() != filter.device_type) {
        return false;
    }
    if (!filter.status.empty() && document.status() != filter.status) {
        return false;
    }
    if (!filter.keyword.empty()
        && !contains(document.title(), filter.keyword)
        && !contains(document.content(), filter.keyword)
        && !contains(document.error_code(), filter.keyword)) {
        return false;
    }
    return true;
}

bool KnowledgeRepository::matchesSearchFilter(const deviceops::db::KnowledgeEntity& entity, const deviceops::knowledge::SearchKnowledgeRequest& request) {
    if (entity.status != "active") {
        return false;
    }
    if (!request.device_type().empty() && nullableString(entity.device_type) != request.device_type()) {
        return false;
    }
    if (!request.error_code().empty() && nullableString(entity.error_code) != request.error_code()) {
        return false;
    }
    return true;
}

double KnowledgeRepository::scoreDocument(const deviceops::db::KnowledgeEntity& entity, const std::string& query) {
    if (query.empty()) {
        return 1.0;
    }

    const auto q = lowercase(query);
    double score = 0.0;
    if (contains(lowercase(entity.title), q)) {
        score += 3.0;
    }
    if (contains(lowercase(entity.category), q)) {
        score += 1.5;
    }
    if (contains(lowercase(nullableString(entity.error_code)), q)) {
        score += 2.0;
    }
    if (contains(lowercase(entity.content), q)) {
        score += 1.0;
    }
    return score;
}

std::string KnowledgeRepository::nullableString(const odb::nullable<std::string>& value) {
    return value ? value.get() : "";
}

bool KnowledgeRepository::contains(const std::string& value, const std::string& keyword) {
    return keyword.empty() || value.find(keyword) != std::string::npos;
}

int KnowledgeRepository::normalizePage(int page) {
    return page <= 0 ? 1 : page;
}

int KnowledgeRepository::normalizePageSize(int page_size) {
    if (page_size <= 0) {
        return 20;
    }
    return page_size > 100 ? 100 : page_size;
}

} // namespace deviceops::knowledge_service
