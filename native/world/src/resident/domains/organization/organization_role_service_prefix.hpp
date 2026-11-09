#pragma once
#include "organization_model.hpp"
#include "resident/model/common_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <algorithm>
#include <cstddef>

namespace hanmo::resident {
enum class OrganizationRoleDutyPrefixState {
    unresolved, no_authored_duty, stopped, active
};
struct OrganizationRoleDutyBeforePrefix {
    const OrgDuty* authored_duty=nullptr;
    OrganizationRoleDutyPrefixState state=OrganizationRoleDutyPrefixState::unresolved;
};

namespace organization_detail {
// This is only the original role-duty identity/coverage consequence. Current
// actor, office, activity, debt and body validation remain with their owners.
inline Status validate_organization_role_duty_provenance(
    const OrganizationState& state,const OrganizationCatalog& catalog){
    for(const auto& row:catalog.duties.rows()){
        const auto& initial=row.second;
        if(initial.role_id.empty())continue;
        const auto* current=state.duties.find(row.first);
        if(!current)return common_failure("MissingSeedOrganizationDuty",row.first);
        if(row.first!=initial.id||current->id!=row.first||
           current->actor_id!=initial.actor_id||
           current->organization_id!=initial.organization_id||
           current->role_id!=initial.role_id)
            return common_failure("InvalidOrganizationDuty",row.first);
    }
    CheckpointVector<const OrgDuty*> roles;
    for(const auto& row:state.duties.rows()){
        const auto& duty=row.second;
        if(duty.role_id.empty())continue;
        const auto* initial=catalog.duties.find(row.first);
        // Genuine named joins create roleless duties. They cannot enlarge this
        // authored role-bearing registry, even when currently inactive.
        if(row.first!=duty.id||!initial||initial->id!=row.first||
           duty.actor_id!=initial->actor_id||
           duty.organization_id!=initial->organization_id||
           duty.role_id!=initial->role_id)
            return common_failure("InvalidOrganizationDuty",row.first);
        roles.push_back(&duty);
    }
    std::sort(roles.begin(),roles.end(),[](const OrgDuty* a,const OrgDuty* b){
        if(a->role_id!=b->role_id)return a->role_id<b->role_id;
        return a->id<b->id;
    });
    for(std::size_t n=1;n<roles.size();++n)
        if(roles[n-1]->role_id==roles[n]->role_id)
            return common_failure("InvalidDutyRole",roles[n]->id);
    return common_success();
}

// This initializes the authored predecessor only. A caller must finish its
// authenticated actual-prefix fold before exposing the result as cut evidence.
inline Outcome<OrganizationRoleDutyBeforePrefix> initialize_organization_role_duty_prefix(
    const OrganizationState& state,const OrganizationCatalog& catalog,
    const StableId& role_id,const StableId& provider_actor_id){
    if(role_id.empty()||provider_actor_id.empty())
        return {false,{},common_failure("OrganizationRoleDutyAuthoredRoleRequired",role_id).error};
    auto valid=validate_organization_role_duty_provenance(state,catalog);
    if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:catalog.duties.rows()){
        const auto& initial=row.second;
        if(initial.role_id!=role_id)continue;
        return {true,{&initial,initial.active?OrganizationRoleDutyPrefixState::active:
            OrganizationRoleDutyPrefixState::stopped},{}};
    }
    return {true,{nullptr,OrganizationRoleDutyPrefixState::no_authored_duty},{}};
}

// The caller supplies the already authenticated Organization parent from its
// sole Common-prefix stream, after the original whole adjunct coverage gate.
// This is one fold of that parent's effects, not a second kind list or scan.
// Parent source authority remains independently required. Any failure discards
// the entire unpublished prefix result, including any retained sink output.
inline Status reduce_organization_role_service_effects_before_prefix(
    const OrganizationFact& parent,const StableId& role_id,
    const StableId& provider_actor_id,OrganizationRoleDutyBeforePrefix& duty,
    bool& enabled,const OrganizationFact*& enabling_parent,
    const OrganizationServiceEffect*& enabling_effect){
    const auto* initial=duty.authored_duty;
    switch(duty.state){
    case OrganizationRoleDutyPrefixState::no_authored_duty:
        if(initial)return common_failure("OrganizationRoleDutyPrefixNotInitialized",role_id);
        break;
    case OrganizationRoleDutyPrefixState::stopped:
    case OrganizationRoleDutyPrefixState::active:
        if(!initial||initial->role_id!=role_id)
            return common_failure("OrganizationRoleDutyPrefixNotInitialized",role_id);
        break;
    default:
        return common_failure("OrganizationRoleDutyPrefixNotInitialized",role_id);
    }
    if(!parent.service_effects)return common_success();
    for(const auto& effect:parent.service_effects->changes){
        if(effect.role_id!=role_id&&(!initial||effect.duty_id!=initial->id))continue;
        if(!initial||effect.duty_id!=initial->id||
           effect.role_id!=initial->role_id||effect.actor_id!=initial->actor_id||
           parent.organization_id!=initial->organization_id)
            return common_failure("OrganizationRoleDutyEffectSourceMismatch",parent.id);
        const bool active=duty.state==OrganizationRoleDutyPrefixState::active;
        if(effect.duty_active_before!=active)
            return common_failure("OrganizationRoleDutyEffectPredecessorMismatch",parent.id);
        if(effect.role_enabled_change&&(effect.actor_id!=provider_actor_id||
           effect.role_enabled_change->before!=enabled))
            return common_failure("InteractionRoleEffectPredecessorMismatch",parent.id);
        duty.state=effect.duty_active_after?OrganizationRoleDutyPrefixState::active:
            OrganizationRoleDutyPrefixState::stopped;
        if(effect.role_enabled_change){
            enabled=effect.role_enabled_change->after;
            enabling_parent=enabled?&parent:nullptr;
            enabling_effect=enabled?&effect:nullptr;
        }
    }
    return common_success();
}
} // namespace organization_detail
} // namespace hanmo::resident
