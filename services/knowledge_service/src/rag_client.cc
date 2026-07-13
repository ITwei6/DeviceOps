#include "knowledge_service/rag_client.h"

#include <cstdlib>
#include <mutex>
#include <sstream>
#include <utility>

#include <curl/curl.h>
#include <jsoncpp/json/json.h>

namespace deviceops::knowledge_service {
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

std::string toJsonBody(const deviceops::knowledge::KnowledgeDocument& document, bool force_rebuild) {
    Json::Value root;
    root["document_id"] = document.document_id();
    root["title"] = document.title();
    root["content"] = document.content();
    root["force_rebuild"] = force_rebuild;
    root["metadata"]["category"] = document.category();
    root["metadata"]["device_type"] = document.device_type();
    root["metadata"]["error_code"] = document.error_code();
    root["metadata"]["status"] = document.status();

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

bool parseTaskId(const std::string& response, std::string* task_id) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(response);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return false;
    }
    if (!root.isMember("task_id")) {
        return false;
    }
    if (task_id != nullptr) {
        *task_id = root["task_id"].asString();
    }
    return true;
}

} // namespace

RagConfig loadRagConfigFromEnv() {
    RagConfig config;
    config.base_url = getenvOrDefault("DEVICEOPS_RAG_URL", config.base_url);
    config.timeout_ms = getenvIntOrDefault("DEVICEOPS_RAG_TIMEOUT_MS", config.timeout_ms);
    return config;
}

RagClient::RagClient(RagConfig config)
    : _config(std::move(config)) {
    _config.base_url = normalizeBaseUrl(_config.base_url);
}

bool RagClient::requestIndex(const deviceops::knowledge::KnowledgeDocument& document, bool force_rebuild, std::string* task_id, std::string* error) const {
    ensureCurlInitialized();

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        if (error != nullptr) {
            *error = "failed to initialize curl";
        }
        return false;
    }

    const std::string url = _config.base_url + "/index";
    const std::string body = toJsonBody(document, force_rebuild);
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
    if (!parseTaskId(response_body, task_id)) {
        if (error != nullptr) {
            *error = "rag-service response missing task_id";
        }
        return false;
    }
    return true;
}

} // namespace deviceops::knowledge_service
