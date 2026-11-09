// Approved DECISIONS.md 2026-10-27 economic GatherPolicy v1, line 34.
// Only exact policy evaluation: original worker/skill/tool proofs, integer
// realization, claims, outputs and condition writes remain with their owners.
#pragma once
#include "resource_site_recovery.hpp"
#include "resident/kernel/source_ref.hpp"
#include <span>
namespace hanmo::resident {
enum class GatherResourceKind {firewood,medicinal,fish_and_game};
enum class GatherMethod {gentle,standard,intensive};
enum class GatherFatigueClass {light,normal,heavy};
struct GatherMethodPolicy {
 GatherMethod method=GatherMethod::gentle;
 Integer harvest_numerator=1,harvest_denominator=2;
 Integer depletion_numerator=1,depletion_denominator=1;
 Integer tool_damage_units=0;
 GatherFatigueClass fatigue=GatherFatigueClass::light;
 bool halves_future_recovery=false;
 Integer recovery_modifier_days=0;
 Integer fatigue_delta_per_completed_half_day=0;
};
// This is a finite compiled rule table, not a string-driven arbitrary policy.
Outcome<GatherMethodPolicy> read_gather_method_policy_v1(GatherMethod);
struct GatherPolicyEvaluation {
 CanonicalFraction exact_potential_harvest;
 GatherMethodPolicy method;
 // A missing required tool is an explicit gentle-only route, not a fallback
 // from a previously accepted standard/intensive method.
 bool missing_required_tool=false;
};
Outcome<GatherPolicyEvaluation> evaluate_gather_policy_v1(
 GatherResourceKind,GatherMethod,bool learned_material_identification,
 bool has_required_tool,const CanonicalFraction& actual_worker_half_days);
// DECISIONS 2026-10-27 supplementary GatherPolicy: sum exact entitlements
// within this action: each actual period pays cumulative floor minus original
// prior receipts. Discard fraction only when the action ends/interupts; no
// second carry ledger. Inputs are all verified completed periods of this action.
struct GatherRealizedQuantity {
 Integer harvested_units=0,site_depletion_units=0,loss_units=0;
};
Outcome<GatherRealizedQuantity> realize_gather_quantity_v1(
 GatherMethod,std::span<const CanonicalFraction> actual_period_entitlements,
 Integer actual_previously_harvested_units,
 Integer remaining_requested_units,Integer current_standing_units,
 Integer remaining_reserved_units,Integer actual_output_capacity_units);
// Borrow original intensive GatherReceipt windows; no independent stacking
// multiplier is persisted. Source validation belongs to the receipt owner.
struct GatherIntensiveRecoveryWindow {
 const SourceRef& gather_receipt_ref;
 const WorldMoment& period_ended_at;
};
struct GatherRecoveryRateView {
 CanonicalFraction effective_rate;
 std::optional<WorldMoment> active_until;
};
Outcome<GatherRecoveryRateView> read_gather_recovery_rate_v1(
 const CanonicalFraction& original_season_rate,const WorldMoment& at,
 std::span<const GatherIntensiveRecoveryWindow> original_intensive_receipts);
}
