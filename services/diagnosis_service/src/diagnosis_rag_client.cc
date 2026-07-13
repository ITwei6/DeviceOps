#include "diagnosis_service/diagnosis_rag_client.h"

#include <cstdlib>
#include <mutex>
#include <sstream>
#include <utility>

#include <curl/curl.h>
#include <jsoncpp/json/json.h>

namespace deviceops::diagnosis_service {
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

void ensureCurlInitialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* output = static_cast<std::string*>(userdata);
    output->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string normalizeBaseUrl(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string writeJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::string arrayJson(std::initializer_list<std::string> values) {
    Json::Value root(Json::arrayValue);
    for (const auto& value : values) {
        if (!value.empty()) {
            root.append(value);
        }
    }
    return writeJson(root);
}

Json::Value buildRequestBody(const deviceops::diagnosis::StartDiagnosisRequest& request,
    const std::optional<deviceops::diagnosis::FaultRecord>& fault) {
    Json::Value root;
    root["event"]["event_id"] = request.event_id();
    root["event"]["device_id"] = request.device_id();
    root["event"]["title"] = request.engineer_note().empty() ? "Device fault diagnosis" : request.engineer_note();
    root["engineer_note"] = request.engineer_note();
    if (fault.has_value()) {
        root["fault"]["fault_id"] = fault->fault_id();
        root["fault"]["device_id"] = fault->device_id();
        root["fault"]["event_id"] = fault->event_id();
        root["fault"]["fault_type"] = fault->fault_type();
        root["fault"]["symptom"] = fault->symptom();
        root["fault"]["status"] = fault->status();
    }
    root["knowledge_snippets"] = Json::arrayValue;
    root["logs"] = Json::arrayValue;
    return root;
}

bool parseResponse(const std::string& body, DiagnosisDraft* draft, std::string* error) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(body);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        if (error != nullptr) {
            *error = errors;
        }
        return false;
    }
    if (draft != nullptr) {
        draft->summary = root["diagnosis"].asString();
        draft->confidence = root["confidence"].asDouble();
        draft->ai_model = root["model"].asString().empty() ? "deviceops-rag-mvp" : root["model"].asString();
        draft->possible_causes_json = arrayJson({"RAG MVP generated diagnosis"});
        draft->recommended_actions_json = arrayJson({"Review event details", "Check device telemetry", "Inspect related logs"});
        Json::Value evidence;
        evidence["rag_response"] = root;
        draft->evidence_json = writeJson(evidence);
    }
    return true;
}

} // namespace

DiagnosisRagConfig loadDiagnosisRagConfigFromEnv() {
    DiagnosisRagConfig config;
    config.base_url = getenvOrDefault("DEVICEOPS_RAG_URL", config.base_url);
    config.timeout_ms = getenvIntOrDefault("DEVICEOPS_RAG_TIMEOUT_MS", config.timeout_ms);
    return config;
}

DiagnosisRagClient::DiagnosisRagClient(DiagnosisRagConfig config)
    : _config(std::move(config)) {
    _config.base_url = normalizeBaseUrl(_config.base_url);
}

bool DiagnosisRagClient::diagnose(const deviceops::diagnosis::StartDiagnosisRequest& request,
    const std::optional<deviceops::diagnosis::FaultRecord>& fault,
    DiagnosisDraft* draft,
    std::string* error) const {
    ensureCurlInitialized();

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        if (error != nullptr) {
            *error = "failed to initialize curl";
        }
        return false;
    }

    const std::string url = _config.base_url + "/diagnose";
    const std::string body = writeJson(buildRequestBody(request, fault));
    std::string response_body;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(_config.timeout_ms));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    const CURLcode code = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        if (error != nullptr) {
            *error = curl_easy_strerror(code);
        }
        return false;
    }
    if (http_code < 200 || http_code >= 300) {
        if (error != nullptr) {
            *error = response_body.empty() ? "rag-service returned non-2xx response" : response_body;
        }
        return false;
    }
    return parseResponse(response_body, draft, error);
}

} // namespace deviceops::diagnosis_service
