#include "interaction_role_history.hpp"
#include "interaction_model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/organization/organization_service_effects_codec.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_contract.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_profile.hpp"
#include <algorithm>
#include <utility>
namespace hanmo::resident {
namespace {
class RolePrefixRead final:public CommonActualFactPrefixSink {
    const RoleDefinition& role_;
    const OrganizationState& organization_;
    const FamilyState& family_;
    bool reception_selected_;
    bool enabled_;
    OriginalMilitaryInteractionRoleBeforePrefix result_;
    organization_detail::OrganizationMemberPrefixFold membership_;
    CommonActualFactPrefixSink* retained_;
public:
    RolePrefixRead(const RoleDefinition& role,const OrganizationState& organization,
        const FamilyState& family,bool reception_selected,
        OrganizationRoleDutyBeforePrefix duty,
        organization_detail::OrganizationMemberPrefixFold&& membership,
        CommonActualFactPrefixSink* retained)
        :role_(role),organization_(organization),family_(family),
         reception_selected_(reception_selected),enabled_(*role.initial_enabled),
         result_{&role,nullptr,nullptr,duty},membership_(std::move(membership)),
         retained_(retained){result_.membership=membership_.result();}
    Status actual_prefix_fact(std::size_t position,const FactRecord& head)override {
        auto observed=membership_.observe_source_head(head);
        if(!observed.ok)return observed;
        if(head.domain=="organization"&&
           (head.kind=="organization_member_response"||
            head.kind=="organization_member_services_synchronized"||
            head.kind=="organization_office_changed")){
            const auto* parent=organization_.facts.find(head.id);
            if(!parent||parent->id!=head.id||parent->kind!=head.kind)
                return common_failure("InteractionRoleOriginalOrganizationParentRequired",head.id);
            // The original whole adjunct coverage/parent gate ran before this
            // read. Ineligible legacy Office parents never acquire effects.
            auto reduced=organization_detail::reduce_organization_role_service_effects_before_prefix(
                *parent,role_.id,role_.initial_actor,result_.duty,enabled_,
                result_.enabling_parent,result_.enabling_effect);
            if(!reduced.ok)return reduced;
            auto member=membership_.apply_parent(head,*parent);
            if(!member.ok)return member;
            result_.membership=membership_.result();
        }else if(head.domain=="family"&&head.kind=="family_succession"){
            const auto* saved=family_.facts.find(head.id);
            const auto* choice=saved?std::get_if<FamilyChoice>(saved):nullptr;
            if(!choice||choice->kind!=head.kind||!choice->assets)
                return common_failure("InteractionRoleOriginalFamilyChoiceRequired",head.id);
            bool listed=false;
            for(const auto& role_id:choice->assets->disabled_roles)if(role_id==role_.id){
                if(listed)return common_failure("InteractionRoleDuplicateFamilyDisable",head.id);
                listed=true;
            }
            // Exactly the original asset_plan role clause, applied to this
            // role's already derived predecessor, never today's RoleRecord.
            const bool expected=enabled_&&choice->guardian_actor_id==role_.initial_actor;
            if(listed!=expected)
                return common_failure("InteractionRoleFamilyDisableCoverageMismatch",head.id);
            if(listed){
                enabled_=false;
                result_.enabling_parent=nullptr;
                result_.enabling_effect=nullptr;
            }
        }else if(head.domain==political_office_reception_domain&&!reception_selected_){
            return common_failure("InteractionRoleReceptionSelectionRequired",head.id);
        }
        return retained_?retained_->actual_prefix_fact(position,head):common_success();
    }
    bool enabled()const noexcept{return enabled_;}
    const OriginalMilitaryInteractionRoleBeforePrefix& result()const noexcept{return result_;}
};
template<class Prefix>
Outcome<OriginalMilitaryInteractionRoleStateBeforePrefix>
read_role_state_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const Prefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    if(service!="military_order"&&service!="office_accept"&&service!="office_resign")
        return {false,{},common_failure("UnsupportedInteractionRoleHistoryService",service).error};
    if(!prepared.common||!prepared.organization||!prepared.interaction||
       prepared.interaction->common.get()!=prepared.common.get())
        return {false,{},common_failure("InteractionRoleOriginalCatalogsRequired",role_id).error};
    const auto* role=prepared.common->roles.find(role_id);
    if(!role||role->id!=role_id||!role->initial_enabled||provider_actor_id.empty()||
       role->initial_actor!=provider_actor_id||
       std::find(role->services.begin(),role->services.end(),service)==role->services.end())
        return {false,{},common_failure("InteractionRoleAuthoredSourceRequired",role_id).error};
    if(!prepared.organization->service_effects_enabled)
        return {false,{},common_failure("InteractionRoleServiceEffectsRequired",role_id).error};
    auto effects=validate_organization_service_effects(organization,1);
    if(!effects.ok)return {false,{},std::move(effects.error)};
    auto reception=political_office_reception_selected_version(prepared.identity.module_versions);
    if(!reception.ok)return {false,{},std::move(reception.error)};
    if(reception.value==1){
        if(!prepared.political_office_receptions||!prepared.political_successions)
            return {false,{},common_failure("InteractionRoleReceptionCatalogsRequired",role_id).error};
        auto valid=validate_political_office_reception_catalog(common,*prepared.common,
            *prepared.organization,*prepared.political_successions,*prepared.political_office_receptions);
        if(!valid.ok)return {false,{},std::move(valid.error)};
    }else if(prepared.political_office_receptions){
        return {false,{},common_failure("UnexpectedInteractionRoleReceptionCatalog",role_id).error};
    }
    auto duty=organization_detail::initialize_organization_role_duty_prefix(
        organization,*prepared.organization,role_id,provider_actor_id);
    if(!duty.ok)return {false,{},std::move(duty.error)};
    organization_detail::OrganizationMemberPrefixFold membership;
    if(duty.value.authored_duty){
        const auto& original_duty=*duty.value.authored_duty;
        auto member_origins=membership.initialize(common,organization,*prepared.organization,
            original_duty.actor_id,original_duty.organization_id);
        if(!member_origins.ok)return {false,{},std::move(member_origins.error)};
    }
    RolePrefixRead reader(*role,organization,family,reception.value==1,duty.value,
        std::move(membership),retained_prefix);
    auto visited=visit_actual_common_fact_prefix(common,prefix,reader);
    if(!visited.ok)return {false,{},std::move(visited.error)};
    return {true,{reader.enabled(),reader.result()},{}};
}
template<class Prefix>
Outcome<OriginalMilitaryInteractionRoleBeforePrefix>
read_role_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const Prefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    auto state=read_role_state_before_prefix(common,prepared,organization,family,
        prefix,role_id,provider_actor_id,service,retained_prefix);
    if(!state.ok)return {false,{},std::move(state.error)};
    if(!state.value.enabled)
        return {false,{},common_failure("InteractionRoleNotEnabledAtOriginalPrefix",role_id).error};
    return {true,state.value.original,{}};
}
} // namespace
Outcome<OriginalMilitaryInteractionRoleStateBeforePrefix>
read_original_military_interaction_role_state_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const CommonActualFactPrefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    return read_role_state_before_prefix(common,prepared,organization,family,prefix,
        role_id,provider_actor_id,service,retained_prefix);
}
Outcome<OriginalMilitaryInteractionRoleStateBeforePrefix>
read_original_military_interaction_role_state_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const CurrentCommonFactPrefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    return read_role_state_before_prefix(common,prepared,organization,family,prefix,
        role_id,provider_actor_id,service,retained_prefix);
}
Outcome<OriginalMilitaryInteractionRoleBeforePrefix>
read_original_military_interaction_role_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const CommonActualFactPrefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    return read_role_before_prefix(common,prepared,organization,family,prefix,
        role_id,provider_actor_id,service,retained_prefix);
}
Outcome<OriginalMilitaryInteractionRoleBeforePrefix>
read_original_military_interaction_role_before_prefix(
    const CommonStore& common,const PreparedCatalog& prepared,
    const OrganizationState& organization,const FamilyState& family,
    const CurrentCommonFactPrefix& prefix,const StableId& role_id,
    const StableId& provider_actor_id,const StableId& service,
    CommonActualFactPrefixSink* retained_prefix){
    return read_role_before_prefix(common,prepared,organization,family,prefix,
        role_id,provider_actor_id,service,retained_prefix);
}
} // namespace hanmo::resident
