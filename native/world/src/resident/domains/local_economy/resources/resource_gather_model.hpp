// E14.3/A06 original ResourceSite owner records. No second Common inventory,
// Action elapsed credit, actor condition, or current clock is stored here.
#pragma once
#include "resource_site_recovery2.hpp"
#include "resident/kernel/fixed_service_work_period.hpp"
namespace hanmo::resident {
struct ResourceGatherAcceptedTerms {
 StableId acceptance_id,claim_id,site_id,action_id,actor_id,item_id;
 StableId recipient_inventory_id;
 MomentOwnerSource harvest_entitlement_source;
 ActorFoundationRuleRef policy_ref;
 GatherMethod method=GatherMethod::gentle;
 Integer requested_units=0,reserved_source_units=0,accepted_half_days=0;
 WorldMoment accepted_at;
 // Frozen original continuation/return policy, not an arbitrary action setter.
 SourceRef return_or_stop_policy;
};
struct ResourceGatherClaim {
 StableId claim_id,accepted_terms_ref;
 Integer remaining_reserved_units=0;
 // Absence means not yet released/closed. Exhaustion alone is not a fabricated
 // Common Action completion; its original owner supplies the terminal source.
 std::optional<SourceRef> terminal_disposition_ref;
};
struct ResourceGatherReceipt {
 StableId receipt_id,claim_id;
 WorkPeriodRef2 actual_period;
 WorldMoment applied_at;
 GatherMethod method=GatherMethod::gentle;
 Integer harvested_units=0,site_depletion_units=0,loss_units=0;
 // Borrowed primitive input proofs are immutable same-job originals. Exact
 // entitlement is recomputed from those periods, never saved as a carry book.
 std::vector<MomentOwnerSource> worker_input_sources;
 std::vector<SourceRef> output_lot_refs,lot_properties_refs;
 std::vector<SourceRef> tool_wear_refs,condition_application_refs;
 std::vector<SourceRef> resource_effect_refs;
 SourceRef observation_result_ref;
 std::optional<SourceRef> zero_output_reason;
 ActorFoundationRuleRef policy_ref;
};
struct ResourceGatherDisposition {
 StableId disposition_id,claim_id;
 SourceRef original_action_disposition;
 WorldMoment occurred_at;
 Integer released_source_units=0;
 // Full close vs true shrink is explicit original provenance. A resize does
 // not pretend the action completed; Common owns its lifecycle.
 bool terminal=false;
};
struct ResourceGatherOwnerState {
 std::vector<ResourceGatherAcceptedTerms> accepted_terms;
 std::vector<ResourceGatherClaim> claims;
 std::vector<ResourceGatherReceipt> receipts;
 std::vector<ResourceGatherDisposition> dispositions;
};
// Structural ownership/once/conservation gate. Full world validation must also
// authenticate each accepted authority, actual Action/period/input/output and
// disposition via the original owners; this is not a standalone provider.
Status validate_resource_gather_owner_values(const ResourceGatherOwnerState&);
}
