#include "diplomacy_delegation_npc.hpp"
namespace hanmo::resident {
Outcome<std::optional<DiplomaticDelegateCommand>> propose_diplomatic_delegation_npc(
    const DiplomaticPresentRead& r,const CommonCatalog& common_catalog,const ActivityRead& activity,
    const StableId& actor,const StableId& policy_id) {
    if(r.catalog.version!=2||r.state.version!=2||!r.catalog.v2||!r.state.v2)
        return {false,{},common_failure("DiplomaticDelegationVersionRequired").error};
    const auto* policy=r.catalog.v2->delegation_policies.find(policy_id);
    if(!policy)return {false,{},common_failure("InvalidDiplomaticDelegationPolicy",policy_id).error};
    if(actor==r.common.state().metadata.player_actor_id)return {true,std::nullopt,{}};
    const auto* office=r.organizations.offices.find(policy->issuer_office_id);
    if(!office||!office->holder_actor_id||*office->holder_actor_id!=actor)return {true,std::nullopt,{}};
    for(const auto& row:r.state.v2->mission_knowledge.rows())if(row.second.delegation_policy_id==policy_id)return {true,std::nullopt,{}};
    DiplomaticDelegateCommand command{policy_id,r.common.revision()};
    auto quote=preview_diplomatic_delegation(r,common_catalog,activity,*r.catalog.v2,*r.state.v2,actor,command);
    if(!quote.ok)return {true,std::nullopt,{}};
    return {true,command,{}};
}
}
