#include "knowledge_service/knowledge_service_impl.h"

#include <brpc/server.h>

namespace deviceops::knowledge_service {
namespace {

void setResponse(deviceops::common::CommonResponse* response, int code, const std::string& message) {
    response->set_code(code);
    response->set_message(message);
}

int pageOrDefault(const deviceops::common::PageRequest& page) {
    return page.page() <= 0 ? 1 : page.page();
}

int pageSizeOrDefault(const deviceops::common::PageRequest& page) {
    if (page.page_size() <= 0) {
        return 20;
    }
    return page.page_size() > 100 ? 100 : page.page_size();
}

} // namespace

KnowledgeServiceImpl::KnowledgeServiceImpl(KnowledgeRepository* repository, RagClient* rag_client)
    : _repository(repository),
      _rag_client(rag_client) {
}

void KnowledgeServiceImpl::CreateKnowledgeDocument(::google::protobuf::RpcController* controller,
    const deviceops::knowledge::CreateKnowledgeDocumentRequest* request,
    deviceops::knowledge::CreateKnowledgeDocumentResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    deviceops::knowledge::KnowledgeDocument document;
    std::string error;
    if (!_repository->create(*request, &document, &error)) {
        setResponse(response->mutable_response(), 400, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_document() = document;
}

void KnowledgeServiceImpl::GetKnowledgeDocument(::google::protobuf::RpcController* controller,
    const deviceops::knowledge::GetKnowledgeDocumentRequest* request,
    deviceops::knowledge::GetKnowledgeDocumentResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto document = _repository->get(request->document_id());
    if (!document.has_value()) {
        setResponse(response->mutable_response(), 404, "document not found");
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    *response->mutable_document() = *document;
}

void KnowledgeServiceImpl::ListKnowledgeDocuments(::google::protobuf::RpcController* controller,
    const deviceops::knowledge::ListKnowledgeDocumentsRequest* request,
    deviceops::knowledge::ListKnowledgeDocumentsResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    KnowledgeListFilter filter;
    filter.page = pageOrDefault(request->page());
    filter.page_size = pageSizeOrDefault(request->page());
    filter.category = request->category();
    filter.device_type = request->device_type();
    filter.keyword = request->keyword();
    filter.status = request->status();

    int64_t total = 0;
    const auto documents = _repository->list(filter, &total);

    setResponse(response->mutable_response(), 0, "ok");
    response->mutable_page()->set_page(filter.page);
    response->mutable_page()->set_page_size(filter.page_size);
    response->mutable_page()->set_total(total);
    for (const auto& document : documents) {
        *response->add_documents() = document;
    }
}

void KnowledgeServiceImpl::SearchKnowledge(::google::protobuf::RpcController* controller,
    const deviceops::knowledge::SearchKnowledgeRequest* request,
    deviceops::knowledge::SearchKnowledgeResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto snippets = _repository->search(*request);

    setResponse(response->mutable_response(), 0, "ok");
    for (const auto& snippet : snippets) {
        *response->add_snippets() = snippet;
    }
}

void KnowledgeServiceImpl::RequestKnowledgeIndex(::google::protobuf::RpcController* controller,
    const deviceops::knowledge::RequestKnowledgeIndexRequest* request,
    deviceops::knowledge::RequestKnowledgeIndexResponse* response,
    ::google::protobuf::Closure* done) {
    (void)controller;
    brpc::ClosureGuard done_guard(done);

    const auto document = _repository->get(request->document_id());
    if (!document.has_value()) {
        setResponse(response->mutable_response(), 404, "document not found");
        return;
    }

    std::string task_id;
    std::string error;
    if (!_rag_client->requestIndex(*document, request->force_rebuild(), &task_id, &error)) {
        setResponse(response->mutable_response(), 503, error);
        return;
    }

    setResponse(response->mutable_response(), 0, "ok");
    response->set_task_id(task_id);
}

} // namespace deviceops::knowledge_service
