// Exact ResourceSite /2 recovery. Does not alter /1 DTO or bytes.
#pragma once
#include "gather_policy_v1.hpp"
namespace hanmo::resident {
struct ResourceSiteRecoveryState2 {
 StableId site_id;Integer remaining_units=0,last_recovery_tick=0,revision=0;
 ActorFoundationRuleRef recovery_rule_ref;
 CanonicalFraction fractional_remainder;
};
struct ResourceRecoveryReceipt2 {
 StableId site_id;Integer from_tick=0,to_tick=0;
 ActorFoundationRuleRef rule_ref;StableId season_id;
 std::vector<SourceRef> intensive_receipt_refs;
 Integer standing_before=0,standing_after=0,recovered_units=0,discarded_whole_units=0;
 CanonicalFraction effective_rate,remainder_before,remainder_after;
 std::vector<MomentOwnerSource> state_source_refs;
};
// Original preflight selects its actual from-tick season and immutable Gather
// receipts. Destruction/claim disposition precedes this phase-current standing.
Outcome<ResourceRecoveryReceipt2> preview_resource_recovery2(
 const ResourceSiteRecoveryState2&,Integer capacity,const ResourceRecoveryRule&,
 const DynamicStepInput&,const ResourceRecoveryDisposition&,
 std::span<const GatherIntensiveRecoveryWindow> original_intensive_receipts);
// Caller owns the one private world candidate and appends the returned original
// receipt. No query/load path calls this writer; no hidden catch-up is permitted.
Outcome<ResourceRecoveryReceipt2> apply_resource_recovery2_in_candidate(
 ResourceSiteRecoveryState2&,Integer capacity,const ResourceRecoveryRule&,
 const DynamicStepInput&,const ResourceRecoveryDisposition&,
 std::span<const GatherIntensiveRecoveryWindow> original_intensive_receipts);
// Cold evidence uses the retained phase-current ORIGINAL before-state and
// preflight inputs. It never reconstructs yesterday from today's standing.
Status validate_resource_recovery_receipt2_against_original(
 const ResourceSiteRecoveryState2&,Integer capacity,const ResourceRecoveryRule&,
 const DynamicStepInput&,const ResourceRecoveryDisposition&,
 std::span<const GatherIntensiveRecoveryWindow>,const ResourceRecoveryReceipt2&);
}
