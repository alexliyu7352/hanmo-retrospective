#include "resident/kernel/catalog_section_observation.hpp"
// 只生成同一原回复命令与计划元信息，公平复评和实际采用归Director唯一事务。
#include "diplomacy_response_director.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
namespace {
Outcome<DiplomaticResponseCandidate> current(const DirectorDiplomaticContext& c,const StableId& actor,const DiplomaticResponsePolicy& policy,Integer limit){
    const auto& p=c.present;DiplomacyViewIndex index;rebuild_diplomacy_view_index({p.common,c.common_catalog,p.organizations,p.organization_catalog,p.catalog,p.state,c.activity},index);
    return propose_diplomatic_response(p,index,actor,policy,limit);
}
}
Status validate_diplomatic_director_catalog(const DirectorCatalog& director,const OrganizationCatalog& organizations,const DiplomacyCatalog* diplomacy,const std::vector<std::pair<StableId,StableId>>& modules){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 94, "DirectorCatalog", "validate_diplomatic_director_catalog", &director);

    Integer markers=0;for(const auto& row:modules)if(row.first=="diplomatic_response"){++markers;if(row.second!="1")return common_failure("UnsupportedDiplomaticResponseVersion");}
    if(markers!=(director.diplomatic_response?1:0))return common_failure("DiplomaticResponseProfileMismatch");
    if(director.diplomatic_response&&(!director.dynamic_subjects||!director.regional_life||!diplomacy||!diplomacy->enabled))return common_failure("DiplomaticResponseProfileRequired");
    const auto& policies=director.dynamic_subjects?director.policies:director.subjects;
    for(const auto& row:policies.rows()){
        const auto& policy=row.second;const bool enabled=std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::diplomatic_response)!=policy.templates.end();
        if(enabled!=policy.diplomatic_response.has_value()||(enabled&&!director.diplomatic_response))return common_failure("InvalidDiplomaticDirectorPolicy",row.first);
        if(!policy.diplomatic_response)continue;
        auto valid=validate_diplomatic_response_policy(organizations,*diplomacy,*policy.diplomatic_response);if(!valid.ok)return valid;
        const auto* office=organizations.offices.find(policy.diplomatic_response->office_id);
        if(!office||policy.organization_id!=office->organization_id)return common_failure("DiplomaticDirectorOrganizationMismatch",row.first);
    }
    return common_success();
}
Status validate_diplomatic_director_history(const DirectorState& director,const DirectorCatalog& catalog,const DiplomacyState* diplomacy){
    const auto plans=[&](const auto& table)->Status{for(const auto& row:table.rows()){
        const auto& plan=row.second;if(plan.template_id!="diplomatic_response")continue;if(plan.subject.kind!=SubjectKind::actor)return common_failure("DiplomaticDirectorActorSubjectRequired",row.first);
        if(!catalog.diplomatic_response||!diplomacy)return common_failure("DiplomaticResponseProfileRequired",row.first);
        const auto* policy=catalog.dynamic_subjects?catalog.policies.find(plan.policy_id):catalog.subjects.find(plan.subject.id);
        const auto* offer=diplomacy->offers.find(plan.goal_ref);
        const auto* presentation=offer?diplomacy->presentations.find(offer->presentation_fact_id):nullptr;
        if(!policy||!policy->diplomatic_response||!offer||!presentation||presentation->representative.actor_id!=plan.subject.id||presentation->representative.office_id!=policy->diplomatic_response->office_id)return common_failure("DiplomaticDirectorSourceMismatch",row.first);
        const auto& directives=policy->diplomatic_response->directives;
        if(std::none_of(directives.begin(),directives.end(),[&](const auto& value){return value.mission_policy_id==offer->policy_id&&value.terms_id==presentation->terms_id;}))return common_failure("DiplomaticDirectorSourceMismatch",row.first);
        if(plan.phase==PlanPhase::completed){bool replied=false;for(const auto& reply:diplomacy->replies.rows())if(reply.second.offer_id==offer->id&&reply.second.presentation_fact_id==presentation->id&&reply.second.authority.actor_id==plan.subject.id)replied=true;if(!replied)return common_failure("DiplomaticDirectorReplyMissing",row.first);}
    }return common_success();};
    auto current=plans(director.plans);if(!current.ok)return current;return plans(director.archived_plans);
}
Outcome<bool> diplomatic_response_available(const DirectorDiplomaticContext& c,const StableId& actor,const DiplomaticResponsePolicy& policy,Integer limit){
    const auto* person=c.present.common.actor(actor);if(!person||!person->alive||!person->available||!person->action_id.empty()||c.activity.in_open_battle(actor))return {true,false,{}};
    auto result=current(c,actor,policy,limit);if(!result.ok)return {false,false,result.error};return {true,result.value.command.has_value(),{}};
}
Outcome<std::optional<DecisionProposal>> propose_director_diplomatic_response(const DirectorDiplomaticContext& c,const DirectorCatalog& director,const KnownSubjectView& known,const DiplomaticResponsePolicy& policy,const DecisionProposal& base){
    const auto& common=c.present.common;if(!base.transition||base.subject_id!=known.actor_id||known.tick!=common.tick()||known.revision!=common.revision()||base.transition->expected_revision!=common.revision())return {false,{},common_failure("InvalidDiplomaticDirectorProposal",known.actor_id,ErrorKind::internal).error};
    auto candidate=current(c,known.actor_id,policy,director.candidates_per_subject);if(!candidate.ok)return {false,{},candidate.error};const auto& choice=candidate.value;if(choice.offer_id.empty())return {true,std::nullopt,{}};
    auto result=base;result.command.reset();auto transition=std::make_shared<DirectorTransition>(*base.transition);transition->template_id="diplomatic_response";transition->goal_ref=choice.offer_id;transition->current_action_id.reset();transition->next_step_index=0;transition->phase=PlanPhase::waiting;transition->wait_reason=choice.waiting_code;result.reason_fact_id=choice.source_fact_id;
    if(choice.command){DiplomacyCommand command{*choice.command};result.command=std::make_shared<const CommandPayload>(CommandPayload{"diplomatic_reply_passage",std::move(command)});transition->phase=PlanPhase::completed;transition->wait_reason.clear();}
    result.transition=std::move(transition);return {true,std::move(result),{}};
}
}
