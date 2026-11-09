#pragma once
#include "repair_policy.hpp"
#include <string_view>
namespace hanmo::resident {
class ActorFoundationJsonVisitor;
class CraftOriginalFactHeaderSink;
bool is_repair_original_fact_kind(std::string_view);
Outcome<Integer> read_repair_original_fact_payload_schema(const CommonStore&,const ItemInstancesState&,const FactRecord&);
Status visit_repair_original_fact_headers(const CommonStore&,const ItemInstancesState&,CraftOriginalFactHeaderSink&);
// Selected Item3 only. Identity bootstrap does not validate service, legal
// title, current custody, or remaining time.
Outcome<const RepairJob*> read_repair_original_action_owner_bootstrap(const CommonStore&,const ItemInstancesState&,const StableId& action_id);
Status validate_repair_original_action_coverage_bootstrap(const CommonStore&,const ItemInstancesState&);
Status read_repair_jobs_json_fields(ActorFoundationJsonVisitor&,const BoundaryValue&,RepairJobsState&,std::string_view);
Outcome<RepairJobsState> decode_repair_jobs(const BoundaryValue&);
Outcome<BoundaryValue> encode_repair_jobs(const RepairJobsState&);
Status emit_repair_jobs(ProjectionWriter&,std::string_view,const RepairJobsState&);
Status visit_repair_jobs(CodecVisitor&,RepairJobsState&);
Status visit_repair_policy_catalog(CodecVisitor&,RepairPolicyCatalog&);
Status emit_repair_original_fact_payload_fields(ProjectionWriter&,const CommonStore&,const ItemInstancesState&,const FactRecord&);
Status emit_repair_original_fact_payload(ProjectionWriter&,const StableId&,const CommonStore&,const ItemInstancesState&,const FactRecord&);
// Full immutable payload equality without building a JSON mirror.
bool same_repair_accepted_terms(const RepairAcceptedTerms&,const RepairAcceptedTerms&);
bool same_repair_terms(const RepairTerms&,const RepairTerms&);
}
