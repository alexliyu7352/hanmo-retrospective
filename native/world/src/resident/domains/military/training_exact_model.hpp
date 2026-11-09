// Fixed S2 §5.2/5.3: original C06 rows live only in military_action_time/1.
// ActionElapsed/2 remains the sole credited-work writer.
#pragma once
#include "training_model.hpp"
#include "military_action_time_records.hpp"
#include "naval_repair_exact_model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
enum class DynamicMilitaryTrainingPhase { started,completed };
enum class DynamicTrainingDrillPhase { pending,frozen,closed };
struct DynamicTrainingDrillInstance2 {
 StableId origin_seed,origin_state;FieldOrder origin_field_order;DrillState challenge;
 DynamicTrainingDrillPhase phase=DynamicTrainingDrillPhase::pending;
 std::optional<WorldMoment> frozen_at;std::optional<Integer> performance_bp;
};
struct OriginalClosingSupplyGate2 {
 Integer lesson_ordinal=0;WorldMoment gate_at;StableId selected_period_ref;
 Integer period_from_tick=0,period_to_tick=0;
 SourceRef opening_receipt_ref,supply_fact_ref;StableId checked_terms_digest;
};
struct TrainingParticipationSpan2 {
 StableId action_ref;SourceRef terms_source_ref;WorldMoment from,to;
 SourceRef actual_participant_selection_ref;
 std::vector<SourceRef> unit_and_occupation_source_refs,eligibility_change_refs;
};
struct TrainingPeriodWitness2 {
 StableId action_ref,period_ref;WorldMoment intersection_from,intersection_to;
 SourceRef opening_receipt_ref;std::vector<SourceRef> actual_participation_span_refs;
 std::optional<SourceRef> observed_supply_ref;
};
struct DynamicMilitaryTrainingLesson2 {
 Integer ordinal=0;WorldMoment service_from,closed_at;WorkUnits gate_work_units;
 std::vector<SourceRef> participation_span_refs,period_witness_refs;
 OriginalClosingSupplyGate2 closing_supply_gate;
 Integer fee_paid=0;SourceRef fee_effect_ref;
 Integer training_before=0,training_after=0,fatigue_before=0,fatigue_after=0;
 std::optional<SourceRef> drill_frozen_source_ref,learning_result_ref;
};
struct DynamicMilitaryTrainingFact2 {
 StableId id,actor_id;DynamicMilitaryTrainingPhase phase=DynamicMilitaryTrainingPhase::started;
 WorldMoment started_at;std::optional<WorldMoment> completed_at;bool success=false;
 MilitaryTrainingTerms terms;ActionElapsedEntryRef elapsed_source_ref;
 ActorActionWorkBinding work_binding_ref;
 std::vector<DynamicMilitaryTrainingLesson2> lessons;
 std::optional<StableId> terminal_code;std::optional<SourceRef> terminal_source_ref;
 std::optional<DynamicTrainingDrillInstance2> drill;Integer rule_version=2;
 // Actual release occurrence, distinct from immutable terminal_source_ref cause.
 // Physical preservation is exclusively military_training_completion_time/1.
 std::optional<SourceRef> completion_application_source;
};
struct TrainingDrillFreezeSource2 {
 StableId action_ref;WorldMoment frozen_at;SourceRef first_lesson_ref;
 Integer performance_bp=0;
};
// Partial typed owner under the one fixed annex. Remaining original action kinds
// must join this owner before its full selected section may be enabled.
struct MilitaryActionTimeState {
 OrderedTable<DynamicMilitaryActionFact2> dynamic_action_facts;
 OrderedTable<DynamicMilitaryLogisticsFact2> dynamic_logistics_facts;
 OrderedTable<DynamicMilitaryReturnFact2> dynamic_return_facts;
 OrderedTable<DynamicMilitaryTrainingFact2> dynamic_training_facts;
 OrderedTable<TrainingParticipationSpan2> training_participation_spans;
 OrderedTable<TrainingPeriodWitness2> training_period_witnesses;
 OrderedTable<TrainingDrillFreezeSource2> drill_freeze_sources;
 OrderedTable<NewNavalRepairTimeSource2> naval_repair_time_sources;
};
}
