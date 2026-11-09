#include "resident/kernel/catalog_section_observation.hpp"
#include "diplomacy_delegation_director.hpp"
#include "resident/kernel/command_types.hpp"
#include <set>
namespace hanmo::resident {
Status validate_diplomatic_delegation_director_catalog(const DirectorCatalog& director,const OrganizationCatalog& organizations,const DiplomacyCatalog* diplomacy) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 93, "DirectorCatalog", "validate_diplomatic_delegation_director_catalog", &director);

    const bool v2=diplomacy&&diplomacy->enabled&&diplomacy->version==2&&diplomacy->v2;
    if(director.diplomatic_delegation!=v2)return common_failure("DiplomaticDelegationProfileMismatch");
    std::set<StableId> used;
    const auto& table=director.dynamic_subjects?director.policies:director.subjects;
    for(const auto& row:table.rows()) {
        const auto& policy=row.second;const bool enabled=std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::diplomatic_delegation)!=policy.templates.end();
        if(enabled!=policy.diplomatic_delegation_policy_id.has_value()||(enabled&&(!v2||!director.dynamic_subjects||!director.regional_life)))
            return common_failure("InvalidDiplomaticDelegationDirectorPolicy",row.first);
        if(!enabled)continue;
        const auto* delegation=diplomacy->v2->delegation_policies.find(*policy.diplomatic_delegation_policy_id);
        const auto* office=delegation?organizations.offices.find(delegation->issuer_office_id):nullptr;
        if(!delegation||!office||office->organization_id!=policy.organization_id||!used.insert(delegation->id).second)
            return common_failure("InvalidDiplomaticDelegationDirectorPolicy",row.first);
    }
    if(v2&&used.size()!=diplomacy->v2->delegation_policies.size())return common_failure("UnboundDiplomaticDelegationPolicy");
    return common_success();
}
Status validate_diplomatic_delegation_director_history(const DirectorState& state,const DirectorCatalog& catalog,const DiplomacyState* diplomacy) {
    const auto validate=[&](const auto& table)->Status{for(const auto& row:table.rows()){
        const auto& plan=row.second;if(plan.template_id!="diplomatic_delegation")continue;if(plan.subject.kind!=SubjectKind::actor)return common_failure("DiplomaticDelegationActorSubjectRequired",row.first);
        if(!catalog.diplomatic_delegation||!diplomacy||diplomacy->version!=2||!diplomacy->v2)return common_failure("DiplomaticDelegationProfileMismatch",row.first);
        const auto* policy=catalog.dynamic_subjects?catalog.policies.find(plan.policy_id):catalog.subjects.find(plan.subject.id);
        if(!policy||!policy->diplomatic_delegation_policy_id||plan.goal_ref!=*policy->diplomatic_delegation_policy_id)
            return common_failure("InvalidDiplomaticDelegationPlan",row.first);
        if(plan.phase==PlanPhase::completed){bool found=false;for(const auto& known:diplomacy->v2->mission_knowledge.rows())if(known.second.delegation_policy_id==plan.goal_ref&&known.second.issuer_actor_id==plan.subject.id)found=true;if(!found)return common_failure("MissingDiplomaticDelegationKnowledge",row.first);}
    }return common_success();};
    auto live=validate(state.plans);if(!live.ok)return live;return validate(state.archived_plans);
}
Outcome<bool> diplomatic_delegation_available(const DirectorDiplomaticContext& c,const StableId& actor,const StableId& policy_id) {
    auto proposed=propose_diplomatic_delegation_npc(c.present,c.common_catalog,c.activity,actor,policy_id);
    if(!proposed.ok)return {false,false,proposed.error};return {true,proposed.value.has_value(),{}};
}
Outcome<std::optional<DecisionProposal>> propose_director_diplomatic_delegation(const DirectorDiplomaticContext& c,const KnownSubjectView& known,
    const StableId& policy_id,const DecisionProposal& base) {
    if(!base.transition||base.subject_id!=known.actor_id||known.tick!=c.present.common.tick()||known.revision!=c.present.common.revision()||base.transition->expected_revision!=c.present.common.revision())
        return {false,{},common_failure("InvalidDiplomaticDelegationProposal").error};
    auto choice=propose_diplomatic_delegation_npc(c.present,c.common_catalog,c.activity,known.actor_id,policy_id);
    if(!choice.ok)return {false,{},choice.error};if(!choice.value)return {true,std::nullopt,{}};
    auto out=base;auto transition=std::make_shared<DirectorTransition>(*base.transition);
    transition->template_id="diplomatic_delegation";transition->goal_ref=policy_id;transition->phase=PlanPhase::completed;transition->wait_reason.clear();
    transition->current_action_id.reset();transition->next_step_index=0;out.reason_fact_id.clear();out.transition=std::move(transition);
    out.command=std::make_shared<const CommandPayload>(CommandPayload{diplomatic_delegate_command_kind,DiplomacyCommand{*choice.value}});
    return {true,std::move(out),{}};
}
}
