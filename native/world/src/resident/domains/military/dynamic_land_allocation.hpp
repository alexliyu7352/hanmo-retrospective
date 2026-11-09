// S2 section 2.3 D/E: one simultaneous land-casualty allocation, no world writes.
// Inputs are the complete already-qualified strike snapshot and real pure quotes.
#pragma once
#include "dynamic_session_model.hpp"
namespace hanmo::resident {
enum class DynamicLandCasualtyRule { legacy_factors,commanded_factors };
struct DynamicLandAllocationUnit {
 StableId unit_ref;std::vector<StableId> original_fit_ids;Integer original_morale=0;
};
struct DynamicLandStrikeDamage {
 SourceRef effect_ref;StableId action_ref,attacker_unit_ref,target_unit_ref;
 Integer action_instance_ordinal=0,effect_ordinal=0,raw_casualties=0,original_flank_morale=0,original_tactic_morale=0;
};
// Only tokens whose original side/range/eligibility source passed in the shared
// strike snapshot appear here. A consumed token remains a real current token,
// but is never selected again. This pure helper grants no qualification.
struct DynamicLandQualifiedSupport {DynamicSupportToken token;};
struct DynamicLandMemberAllocation {
 StableId member_id,unit_ref;SourceRef effect_ref;StableId direct_target_unit_ref;
 bool support_share=false;StableId resulting_status;
};
struct DynamicLandStrikeAllocation {
 SourceRef effect_ref;Integer raw_casualties=0,actual_target_count=0,actual_support_count=0;
 std::optional<SourceRef> consumed_support_activation_ref;
 std::optional<StableId> supporter_unit_ref;
 std::vector<StableId> target_member_ids,support_member_ids;
};
struct DynamicLandMoraleChange {StableId unit_ref;Integer before_morale=0,delta=0,after_morale=0;};
struct DynamicLandMergedAllocation {
 std::vector<DynamicLandStrikeAllocation> strikes;
 std::vector<DynamicLandMemberAllocation> member_allocations;
 std::vector<DynamicLandMoraleChange> morale_changes;
};
Outcome<DynamicLandMergedAllocation> merge_dynamic_land_member_allocations(
 const std::vector<DynamicLandAllocationUnit>& strike_snapshot,
 const std::vector<DynamicLandStrikeDamage>& complete_canonical_strikes,
 const std::vector<DynamicLandQualifiedSupport>& actual_qualified_supports,
 DynamicLandCasualtyRule);
Status validate_dynamic_land_member_allocations(
 const std::vector<DynamicLandAllocationUnit>&,
 const std::vector<DynamicLandStrikeDamage>&,
 const std::vector<DynamicLandQualifiedSupport>&,
 DynamicLandCasualtyRule,const DynamicLandMergedAllocation&);
}
