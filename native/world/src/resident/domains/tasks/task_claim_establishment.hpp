// T§4: the accepted claim is established at start, never at the later acceptance Action terminal.
#pragma once
#include <string_view>
#include "resident/model/dynamic_contract.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct TaskState;struct AffairsState;struct CodecVisitor;struct TaskExactLifeSources;
struct LegacyClaimAcceptedTick { Integer accepted_tick=0; };
struct TaskClaimEstablishedRef { SourceRef source; Integer source_schema=1; };
using NeedClaimAcceptanceSource=std::variant<LegacyClaimAcceptedTick,TaskClaimEstablishedRef>;
// Fixed TI2.3 / S3 5.2 seven-field original row. The committed result is
// bound only after the real Common seal in the same unpublished candidate.
struct TaskClaimEstablishedSource {
 StableId source_id,task_id,contract_id,claim_id,acceptance_action_id;
 WorldMoment established_at;SourceRef committed_result_ref;
};
struct TaskClaimEstablishmentState { OrderedTable<TaskClaimEstablishedSource> claim_establishment_sources; };
Outcome<SourceRef> establish_task_claim_source(CommonStore&,TaskState&,AffairsState&,const StableId& task_id,const StableId& accept_action_id,const WorldMoment&);
Outcome<const TaskClaimEstablishedSource*> read_task_claim_established_source(const CommonStore&,const TaskState&,const AffairsState&,const TaskClaimEstablishedRef&,const TaskExactLifeSources* life=nullptr);
Status bind_task_claim_established_result_in_candidate(const CommonStore&,TaskState&,const AffairsState&,const StableId& acceptance_action_id,const SourceRef& actual_committed_result,const TaskExactLifeSources* life=nullptr);
// Non-recursive source-time leg for P10 capture before the actual Common seal.
// Full cold/result consumers use read_task_claim_established_source above.
Outcome<WorldMoment> read_task_claim_established_moment(const CommonStore&,const TaskState&,const AffairsState&,const TaskClaimEstablishedRef&);
Outcome<WorldMoment> read_need_claim_acceptance_at(const CommonStore&,const TaskState&,const AffairsState&,const StableId& claim_id,const TaskExactLifeSources* life=nullptr);
Status validate_task_claim_establishment(const CommonStore&,const TaskState&,const AffairsState&,const TaskExactLifeSources* life=nullptr);
Outcome<TaskClaimEstablishmentState> decode_task_claim_establishment(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_claim_establishment(const TaskClaimEstablishmentState&);
Status visit_task_claim_establishment_checkpoint(CodecVisitor&,TaskClaimEstablishmentState&);
class ProjectionWriter;
Outcome<bool> emit_task_claim_established_fact_payload(ProjectionWriter&,const TaskState&,const FactRecord&);
Status emit_task_claim_establishment_projection(ProjectionWriter&,std::string_view,const TaskClaimEstablishmentState&);
}
