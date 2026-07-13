#pragma once

#include <optional>
#include <string>

#include "diagnosis.pb.h"

namespace deviceops::diagnosis_service {

struct DiagnosisRagConfig {
    std::string base_url = "http://127.0.0.1:9601";
    int timeout_ms = 5000;
};

struct DiagnosisDraft {
    std::string summary;
    std::string possible_causes_json;
    std::string recommended_actions_json;
    std::string evidence_json;
    std::string ai_model = "deviceops-rag-mvp";
    double confidence = 0.0;
};

DiagnosisRagConfig loadDiagnosisRagConfigFromEnv();

class DiagnosisRagClient {
public:
    explicit DiagnosisRagClient(DiagnosisRagConfig config);

    bool diagnose(const deviceops::diagnosis::StartDiagnosisRequest& request,
        const std::optional<deviceops::diagnosis::FaultRecord>& fault,
        DiagnosisDraft* draft,
        std::string* error) const;

private:
    DiagnosisRagConfig _config;
};

} // namespace deviceops::diagnosis_service
