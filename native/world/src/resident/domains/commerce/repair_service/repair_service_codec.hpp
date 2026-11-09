#pragma once
#include "repair_service_model.hpp"
#include <string_view>
namespace hanmo::resident {
class ActorFoundationJsonVisitor;
Status read_repair_service_contracts_json_fields(ActorFoundationJsonVisitor&,const BoundaryValue&,RepairServiceContractsState&,std::string_view);
class CodecVisitor;class ProjectionWriter;class CommonStore;struct ItemInstancesState;
class RepairServiceOriginalFactHeaderSink {
public:
 virtual ~RepairServiceOriginalFactHeaderSink()=default;
 virtual Status append(const FactRecord& actual_header,Integer original_schema)=0;
};
bool is_repair_service_original_fact_kind(std::string_view);
Outcome<Integer> read_repair_service_original_payload_schema(const CommonStore&,const ItemInstancesState&,const FactRecord&);
Status visit_repair_service_original_fact_headers(const CommonStore&,const ItemInstancesState&,RepairServiceOriginalFactHeaderSink&);
Outcome<RepairServiceContractsState> decode_repair_service_contracts(const BoundaryValue&);
Outcome<BoundaryValue> encode_repair_service_contracts(const RepairServiceContractsState&);
Status emit_repair_service_contracts_fields(ProjectionWriter&,const RepairServiceContractsState&);
Status emit_repair_service_contracts(ProjectionWriter&,std::string_view,const RepairServiceContractsState&);
Status visit_repair_service_contracts(CodecVisitor&,RepairServiceContractsState&);
Status emit_repair_service_original_payload_fields(ProjectionWriter&,const CommonStore&,const ItemInstancesState&,const FactRecord&);
Status emit_repair_service_original_payload(ProjectionWriter&,const StableId&,const CommonStore&,const ItemInstancesState&,const FactRecord&);
bool same_repair_service_quoted_terms(const RepairServiceQuotedTerms&,const RepairServiceQuotedTerms&);
bool same_repair_service_inspection(const RepairServiceAcceptanceInspection&,const RepairServiceAcceptanceInspection&);
bool same_repair_service_contract_terms(const RepairServiceContractTerms&,const RepairServiceContractTerms&);
}
