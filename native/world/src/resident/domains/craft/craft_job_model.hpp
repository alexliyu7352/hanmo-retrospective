// S9原作业只保存专业接受、工序与结果；累计工量和赶工量唯一在S0 SharedWork。
#pragma once
#include "craft_quality.hpp"
#include "craft_facility_occupancy_model.hpp"
#include "craft_facility_boundary_model.hpp"
#include "craft_cancellation_model.hpp"
#include "resident/kernel/shared_work_runtime.hpp"
#include "resident/facility_claims/facility_claims.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct CraftMaterialSelection {
 Integer material_index=0,alternative_index=0;CargoSelection selected;Owner legal_owner;
 // 客供料必须已有真实加工保管/用途接受，不能靠Owner相同放行。
 std::optional<MomentOwnerSource> custody_use_source;
};
struct CraftTerms {
 StableId job_id;CraftRecipeRef recipe_ref;CraftBrief brief;
 SubjectRef maker;std::vector<StableId> accepted_helpers;
 FacilitySlotRef workstation;std::optional<SourceRef> order_ref;
 std::vector<CraftMaterialSelection> material_selections;
 std::optional<SourceRef> payer_ref,payee_ref;SourceRef fee_terms_ref;
 Integer normal_work_ticks=0;CraftPace pace=CraftPace::normal;CraftMode mode=CraftMode::automatic;
 CraftQualityInputs quality_inputs;StableId cancellation_policy,result_owner_policy;
 Owner result_owner;StableId output_inventory_id;ReservationRef material_holder;
 SourceRef acceptance_ref;WorldMoment accepted_at;Integer terms_revision=1;
 std::vector<MomentOwnerSource> qualification_sources,workstation_sources;
 // Actual accepting heads only: references into S4, not another body ledger.
 std::vector<ActorConditionHead> participant_condition_heads;
};
// 原ReserveSelectedLots真实映射；是接受证据而非可推进的余料数量账。
struct CraftReservedMaterial {
 Integer selection_ordinal=0;CargoSelection original,held;StableId definition_id;
 Owner original_owner;std::optional<LotPropertiesRef> properties_ref;
};
struct CraftConsumedMaterial {
 Integer selection_ordinal=0,material_index=0;CargoSelection actual;
 StableId definition_id;Owner original_owner;std::optional<LotPropertiesRef> properties_ref;
};
struct CraftStageResult {
 StableId id,job_id,gate_result_id;Integer ordinal=0;
 WorldMoment occurred_at;std::vector<CraftConsumedMaterial> consumed;
};
struct CraftServiceSpan {
 StableId id,job_id;Integer gate_ordinal=0;WorkServiceIntervalProof actual;
};
// Borrowed inputs for the actual service-span copy. No persistent fields change.

struct CraftServiceSpanBuildView {
 const StableId& id;
 const StableId& job_id;
 Integer gate_ordinal;
 const WorkServiceIntervalProof& actual;
};

struct CraftGateApplied {
 StableId id,job_id;WorldMoment occurred_at;CraftWorkGateResult result;
 std::vector<StableId> stage_result_refs;
};
struct CraftQualityResult {
 StableId id,job_id,final_gate_ref;WorldMoment occurred_at;CraftQualityValue value;
};
// 原玩家/合法业务已采用的未来pace改变，无工量或R副本。
struct CraftPaceChange {
 StableId id,job_id,actor_id;Integer pace_revision=0,gate_ordinal=0;
 std::optional<StableId> last_closed_gate_ref;
 CraftPace previous_pace=CraftPace::normal,pace=CraftPace::normal;
 WorldMoment occurred_at;std::optional<MomentOwnerSource> original_contract_change;
};
enum class CraftControlKind {pause,resume};
// Real owner transition only. No duplicate elapsed/earned/W/R values are kept.
struct CraftControlSource {
 StableId id,job_id,actor_id;Integer revision=0,gate_ordinal=0;
 std::optional<StableId> last_closed_gate_ref;CraftControlKind kind=CraftControlKind::pause;
 WorldMoment occurred_at;std::optional<MomentOwnerSource> actual_cause;
 std::vector<MomentOwnerSource> actual_admission_sources;
 std::vector<StableId> actual_claim_refs;
 std::optional<CraftFacilityClaimUntil> facility_until;
};
struct CraftCompletionResult {
 StableId id,job_id,quality_result_ref;WorldMoment occurred_at;
 StableId destination_inventory_id;ItemMaterialization output;
 std::vector<SourceRef> actual_practice_sources;
};
enum class CraftJobPhase {reserved,working,paused,awaiting_output_space,completed,cancelling,settling,cancelled,failed};
struct CraftJob {
 StableId id;CraftTerms terms;ActorActionWorkBinding execution_binding;
 Integer current_ordinal=0;std::optional<StableId> last_consumed_gate_ref;
 std::vector<CraftReservedMaterial> original_reservations;
 std::vector<StableId> actual_service_refs,stage_result_refs,gate_result_refs;
 std::vector<StableId> facility_claim_refs,pace_change_refs,control_refs;
 std::vector<StableId> facility_occupancy_event_refs;
 std::optional<StableId> quality_result_ref,completion_fact_id;
 std::optional<StableId> cancellation_ref,cancelled_result_ref;
 CraftJobPhase phase=CraftJobPhase::reserved;
};
struct CraftJobsState {
 Integer schema_version=1;OrderedTable<CraftJob> jobs;
 OrderedTable<CraftServiceSpan> service_spans;OrderedTable<CraftStageResult> stages;
 OrderedTable<CraftGateApplied> gates;OrderedTable<CraftQualityResult> qualities;
 OrderedTable<CraftCompletionResult> completions;OrderedTable<CraftPaceChange> pace_changes;
 OrderedTable<CraftControlSource> controls;
 OrderedTable<CraftCancellationSource> cancellations;
 OrderedTable<CraftCancellationResult> cancellation_results;
 OrderedTable<CraftFacilityBoundarySource> facility_boundaries;
 OrderedTable<CraftFacilityOccupancyEvent> facility_occupancy_events;
};
// Source/schema固定，实际源仍须从该唯一专业拥有表反读。
SourceRef craft_job_source(const StableId&);
SourceRef craft_stage_source(const StableId&);
SourceRef craft_gate_source(const StableId&);
SourceRef craft_quality_source(const StableId&);
SourceRef craft_completion_source(const StableId&);
SourceRef craft_pace_change_source(const StableId&);
SourceRef craft_control_source(const StableId&);
ReservationRef craft_job_reservation(const StableId&);
}
