// S2 §5.5 C21 / §6.2: one exact leaf under military_action_time/1.
#pragma once
#include "naval_repair_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct DynamicNavalRepairTiming2 {
 SourceRef start_fact_ref;StableId action_ref;WorldMoment started_at;
 std::optional<WorldMoment> completion_or_interruption_at;
 ActionElapsedEntryRef elapsed_ref;std::optional<SourceRef> result_ref;
 std::vector<SourceRef> charge_source_refs;
};
// Original result fields retain their meaning. Its exact occurrence is owned
// once by timing.completion_or_interruption_at, never reconstructed from tick.
struct ExactNavalRepairResult2 {
 StableId id,action_id,actor_id,request_key,reason,interruption_source_id;
 bool success=false;
 // Crew/ownership/contact changes have a real original cause. The legacy
 // interruption_source_id remains empty for CrewChanged, as in its original.
 std::optional<MomentOwnerSource> actual_interruption_source;
};
struct NewNavalRepairTimeSource2 {
 DynamicNavalRepairTiming2 timing;
 StableId request_key;NavalRepairTerms original_terms;
 std::optional<ExactNavalRepairResult2> original_result;
 std::optional<NavalRepairChargeFact> original_charge;
 Integer rule_version=2;
};
// Nonpersistent command return. The actual immutable sources above own time.
struct ExactNavalRepairReceipt {
 SourceRef source_ref;StableId action_id,actor_id,reason;WorldMoment occurred_at;
 bool completed=false,success=false,replayed=false;
};
}
