#pragma once

#include "diagnosis.pb.h"
#include "diagnosis_service/diagnosis_rag_client.h"
#include "diagnosis_service/diagnosis_repository.h"

namespace deviceops::diagnosis_service {

class DiagnosisServiceImpl final : public deviceops::diagnosis::DiagnosisService {
public:
    DiagnosisServiceImpl(DiagnosisRepository* repository, DiagnosisRagClient* rag_client);

    void CreateFaultRecord(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::CreateFaultRecordRequest* request,
        deviceops::diagnosis::CreateFaultRecordResponse* response,
        ::google::protobuf::Closure* done) override;
    void GetFaultRecord(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::GetFaultRecordRequest* request,
        deviceops::diagnosis::GetFaultRecordResponse* response,
        ::google::protobuf::Closure* done) override;
    void ListFaultRecords(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::ListFaultRecordsRequest* request,
        deviceops::diagnosis::ListFaultRecordsResponse* response,
        ::google::protobuf::Closure* done) override;
    void StartDiagnosis(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::StartDiagnosisRequest* request,
        deviceops::diagnosis::StartDiagnosisResponse* response,
        ::google::protobuf::Closure* done) override;
    void GetDiagnosisReport(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::GetDiagnosisReportRequest* request,
        deviceops::diagnosis::GetDiagnosisReportResponse* response,
        ::google::protobuf::Closure* done) override;
    void ListDiagnosisReports(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::ListDiagnosisReportsRequest* request,
        deviceops::diagnosis::ListDiagnosisReportsResponse* response,
        ::google::protobuf::Closure* done) override;
    void ConfirmDiagnosisReport(::google::protobuf::RpcController* controller,
        const deviceops::diagnosis::ConfirmDiagnosisReportRequest* request,
        deviceops::diagnosis::ConfirmDiagnosisReportResponse* response,
        ::google::protobuf::Closure* done) override;

private:
    DiagnosisRepository* _repository;
    DiagnosisRagClient* _rag_client;
};

} // namespace deviceops::diagnosis_service
