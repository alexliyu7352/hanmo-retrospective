// Original military-service role value at a genuine retained Common prefix.
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/organization/organization_role_service_prefix.hpp"
#include "resident/domains/organization/organization_member_prefix.hpp"
namespace hanmo::resident {
struct RoleDefinition;
struct OrganizationState;
struct OrganizationFact;
struct OrganizationServiceEffect;
struct FamilyState;
struct OriginalMilitaryInteractionRoleBeforePrefix {
    const RoleDefinition* authored_role=nullptr;
    // Null means the authored enabled seed remains the final enabling source.
    // These are borrowed actual rows, not a new SourceRef/schema or full history.
    const OrganizationFact* enabling_parent=nullptr;
    const OrganizationServiceEffect* enabling_effect=nullptr;
    OrganizationRoleDutyBeforePrefix duty;
    OrganizationMemberBeforePrefix membership;
};
// Non-persistent result of the same complete source fold. A disabled role is
// a valid historical value, not an error or authority to bypass a failed source.
struct OriginalMilitaryInteractionRoleStateBeforePrefix {
    bool enabled=false;
    OriginalMilitaryInteractionRoleBeforePrefix original;
};
Outcome<OriginalMilitaryInteractionRoleStateBeforePrefix>
read_original_military_interaction_role_state_before_prefix(
    const CommonStore&,const PreparedCatalog&,const OrganizationState&,
    const FamilyState&,const CommonActualFactPrefix&,
    const StableId& role_id,const StableId& provider_actor_id,
    const StableId& service,CommonActualFactPrefixSink* retained_prefix=nullptr);
Outcome<OriginalMilitaryInteractionRoleStateBeforePrefix>
read_original_military_interaction_role_state_before_prefix(
    const CommonStore&,const PreparedCatalog&,const OrganizationState&,
    const FamilyState&,const CurrentCommonFactPrefix&,
    const StableId& role_id,const StableId& provider_actor_id,
    const StableId& service,CommonActualFactPrefixSink* retained_prefix=nullptr);
// Only the original military_order, office_accept and office_resign services
// are covered. The actual selected reception catalog excludes these roles.
// The accepting owner must prove that prefix was captured at its real check.
// This leaf proves neither contact/body/consent nor Action/battle absence.
// All roots and returned pointers must remain in the same unchanged candidate.
// The entire prefix, not merely enabling_parent, must remain retained. Optional
// sink receives that same actual prefix during this one traversal; it must not
// mutate any source root and must discard partial output if this read fails.
Outcome<OriginalMilitaryInteractionRoleBeforePrefix>
read_original_military_interaction_role_before_prefix(
    const CommonStore&,const PreparedCatalog&,const OrganizationState&,
    const FamilyState&,const CommonActualFactPrefix&,
    const StableId& role_id,const StableId& provider_actor_id,
    const StableId& service,CommonActualFactPrefixSink* retained_prefix=nullptr);
Outcome<OriginalMilitaryInteractionRoleBeforePrefix>
read_original_military_interaction_role_before_prefix(
    const CommonStore&,const PreparedCatalog&,const OrganizationState&,
    const FamilyState&,const CurrentCommonFactPrefix&,
    const StableId& role_id,const StableId& provider_actor_id,
    const StableId& service,CommonActualFactPrefixSink* retained_prefix=nullptr);
} // namespace hanmo::resident
