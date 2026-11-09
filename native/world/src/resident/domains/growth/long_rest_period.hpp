// G+64 only. One immutable original long_rest consumption, embedded in
// RestFact. Progress/earned time remains exclusively in ActionElapsed;
// elapsed/consumed remain the original RestFact projections and are checked
// against these rows.
#pragma once
#include <string_view>
#include "resident/kernel/world_timeline.hpp"
#include "resident/domains/estate/estate_rest_adoption.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct LongRestMouthInputs {
 StableId actor_id;
 // Owner has no adoption. Other mouths name the original completed adoption,
 // not a reconstructed parent relation or a present-day child list.
 std::optional<MomentOwnerSource> original_adoption;
 ActorConditionHead condition_before;
 std::vector<MomentOwnerSource> actual_location_sources;
};
struct LongRestActualPeriod {
 Integer ordinal=0;WorldMoment provided_from,applied_at;
 // Actual pre-application FSL snapshots, in the original service-chain order.
 std::vector<StableId> input_boundary_refs;
 EstateRestAdoptionBasis estate_basis;
 std::vector<StableId> mouth_actor_ids;
 StableId inventory_id,food_item_id;Integer consumed_quantity=0;
 // Same order as actual original per-mouth apply_actor calls, including net zero.
 std::vector<ConditionReceiptRefV2> condition_receipt_refs;
 // G+128 only. Complete original candidate set, sorted by actor ID;
 // actual mouth_actor_ids are the original eligible subset, not all candidates.
 std::vector<LongRestMouthInputs> mouth_inputs;
};
class CodecVisitor;class ProjectionWriter;
Status visit_long_rest_actual_period(CodecVisitor&,LongRestActualPeriod&,bool exact_rest_mouths=false);
Status emit_long_rest_actual_period(ProjectionWriter&,std::string_view,const LongRestActualPeriod&,bool exact_rest_mouths=false);
Outcome<LongRestActualPeriod> decode_long_rest_actual_period(const BoundaryValue&,bool exact_rest_mouths=false);
}
