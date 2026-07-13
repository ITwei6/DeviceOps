#pragma once

#include <string>

#include "knowledge.pb.h"

namespace deviceops::knowledge_service {

struct RagConfig {
    std::string base_url = "http://127.0.0.1:9601";
    int timeout_ms = 3000;
};

RagConfig loadRagConfigFromEnv();

class RagClient {
public:
    explicit RagClient(RagConfig config);

    bool requestIndex(const deviceops::knowledge::KnowledgeDocument& document, bool force_rebuild, std::string* task_id, std::string* error) const;

private:
    RagConfig _config;
};

} // namespace deviceops::knowledge_service
