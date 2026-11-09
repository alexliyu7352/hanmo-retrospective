// Craft原拥有段与每个Common payload共用同一字段表。冷存读不执行工序。
#pragma once
#include "craft_job_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;
class CraftOriginalFactHeaderSink {
public:
 virtual ~CraftOriginalFactHeaderSink()=default;
 virtual Status append(const FactRecord& actual_header,Integer original_schema)=0;
};
// Bootstrap identity only. These borrow decoded roots before the original
// whole-world authority validator is constructible; no source is authorized.
bool is_craft_original_fact_kind(std::string_view);
Outcome<Integer> read_craft_original_fact_payload_schema(const CommonStore&,const CraftJobsState&,const FactRecord&);
Status visit_craft_original_fact_headers(const CommonStore&,const CraftJobsState&,CraftOriginalFactHeaderSink&);
// Original accepted parameters remain in CraftTerms. This bootstrap proves
// only the current Action's typed owner/identity, never work or authority.
Outcome<const CraftJob*> read_craft_original_action_owner_bootstrap(const CommonStore&,const CraftJobsState&,const StableId& action_id);
Status validate_craft_original_action_coverage_bootstrap(const CommonStore&,const CraftJobsState&);
Outcome<CraftJobsState> decode_craft_jobs(const BoundaryValue&);
Outcome<BoundaryValue> encode_craft_jobs(const CraftJobsState&);
Status visit_craft_jobs_checkpoint(CodecVisitor&,CraftJobsState&);
Status emit_craft_jobs_projection(ProjectionWriter&,const StableId&,const CraftJobsState&);
Status emit_craft_original_fact_payload_fields(ProjectionWriter&,const CommonStore&,const CraftJobsState&,const FactRecord&);
Status emit_craft_original_fact_payload(ProjectionWriter&,const StableId&,const CommonStore&,const CraftJobsState&,const FactRecord&);
}
