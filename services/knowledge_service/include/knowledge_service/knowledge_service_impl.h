#pragma once

#include "deviceops/mq/rabbitmq_event_publisher.h"
#include "knowledge.pb.h"
#include "knowledge_service/knowledge_repository.h"
#include "knowledge_service/rag_client.h"

namespace deviceops::knowledge_service {

class KnowledgeServiceImpl final : public deviceops::knowledge::KnowledgeService {
public:
    KnowledgeServiceImpl(KnowledgeRepository* repository,
        RagClient* rag_client,
        deviceops::mq::RabbitMqEventPublisher* event_publisher);

    void CreateKnowledgeDocument(::google::protobuf::RpcController* controller,
        const deviceops::knowledge::CreateKnowledgeDocumentRequest* request,
        deviceops::knowledge::CreateKnowledgeDocumentResponse* response,
        ::google::protobuf::Closure* done) override;

    void GetKnowledgeDocument(::google::protobuf::RpcController* controller,
        const deviceops::knowledge::GetKnowledgeDocumentRequest* request,
        deviceops::knowledge::GetKnowledgeDocumentResponse* response,
        ::google::protobuf::Closure* done) override;

    void ListKnowledgeDocuments(::google::protobuf::RpcController* controller,
        const deviceops::knowledge::ListKnowledgeDocumentsRequest* request,
        deviceops::knowledge::ListKnowledgeDocumentsResponse* response,
        ::google::protobuf::Closure* done) override;

    void SearchKnowledge(::google::protobuf::RpcController* controller,
        const deviceops::knowledge::SearchKnowledgeRequest* request,
        deviceops::knowledge::SearchKnowledgeResponse* response,
        ::google::protobuf::Closure* done) override;

    void RequestKnowledgeIndex(::google::protobuf::RpcController* controller,
        const deviceops::knowledge::RequestKnowledgeIndexRequest* request,
        deviceops::knowledge::RequestKnowledgeIndexResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    KnowledgeRepository* _repository;
    RagClient* _rag_client;
    deviceops::mq::RabbitMqEventPublisher* _event_publisher;
};

} // namespace deviceops::knowledge_service
