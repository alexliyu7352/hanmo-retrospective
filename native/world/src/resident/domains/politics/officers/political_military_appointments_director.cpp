// 受命本人只消费已交到自己手中的军令；远方只按军令作者营地行旅，不远读现任真人位置。
#include "political_military_appointments_director.hpp"
#include "political_military_appointments_detail.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_military_appointment(const PoliticalMilitaryAppointmentContext& c,const TravelCatalog& travel,const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){
    using Result=Outcome<std::optional<DecisionProposal>>;
    if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor||known.subject.id!=known.actor_id||known.tick!=c.common.tick()||known.revision!=c.common.revision()||base.transition->expected_revision!=known.revision)return {false,{},common_failure("InvalidMilitaryDirectorProposal",known.actor_id,ErrorKind::internal).error};
    if(known.actor_id==c.common.state().metadata.player_actor_id||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)return {true,std::nullopt,{}};
    auto audit=political_military_detail::audit(c);if(!audit.ok)return {false,{},audit.error};Integer scanned=0;
    for(const auto& row:c.state.authorizations.rows()){
        const auto& issued=row.second;if(issued.target.next_actor_id!=known.actor_id||issued.target.organization_id!=policy.organization_id||audit.value.withdrawals.count(issued.id))continue;
        const auto found=audit.value.answers.find(issued.id);const PoliticalMilitaryAnswer* answer=found==audit.value.answers.end()?nullptr:found->second;
        if(++scanned>256)break;if(answer&&(!answer->accepted||c.common.tick()>=answer->apply_before_tick))continue;if(!answer&&c.common.tick()>=issued.answer_before_tick)continue;
        auto out=base;auto next=std::make_shared<DirectorTransition>(*base.transition);out.transition=next;next->template_id="military_appointment";next->goal_ref=issued.id;next->current_action_id.reset();next->next_step_index=0;next->phase=PlanPhase::completed;next->wait_reason.clear();out.reason_fact_id=issued.id;
        if(!answer){PoliticalMilitaryAppointmentCommand command=PoliticalMilitaryAnswerCommand{issued.id,true};auto quoted=preview_political_military_appointment(c,known.actor_id,command);if(!quoted.ok){if(quoted.error.kind!=ErrorKind::rule)return {false,{},quoted.error};continue;}out.command=std::make_shared<const CommandPayload>(CommandPayload{"political_military_officer_answer",std::move(command)});return {true,std::move(out),{}};}
        auto proof=PoliticalMilitaryAppointmentAccess(c).appointment_for_application(issued.id,answer->id,known.actor_id);if(!proof.ok){if(proof.error.kind!=ErrorKind::rule)return {false,{},proof.error};continue;}
        const auto* destination=c.catalog.policies.find(issued.policy_id);if(!destination)return {false,{},common_failure("PoliticalMilitaryPolicyMissing").error};
        if(known.place_id!=destination->site_id){out.command=director_move_toward(travel,known,destination->site_id);next->phase=out.command?PlanPhase::delivering:PlanPhase::waiting;next->wait_reason=out.command?StableId{}:"PoliticalMilitaryCampRouteUnavailable";return {true,std::move(out),{}};}
        if(!c.units.application_preview)return {false,{},common_failure("PoliticalMilitaryApplicationReadMissing",{},ErrorKind::internal).error};auto actual=c.units.application_preview(known.actor_id,issued.id,answer->id);if(!actual.ok){if(actual.error.kind!=ErrorKind::rule)return {false,{},actual.error};continue;}
        MilitaryCommand command=MilitaryOfficerAssign{"appointment:"+issued.id,issued.id,answer->id,{}};out.command=std::make_shared<const CommandPayload>(CommandPayload{"military_officer_assign",std::move(command)});out.reason_fact_id=answer->id;return {true,std::move(out),{}};
    }
    return {true,std::nullopt,{}};
}
// 新计划只能绑定本人真实军令；完成签受不等于军事已交接，军事实际历史另由S2严格验证。
Status validate_political_military_director(const DirectorCatalog& catalog,const DirectorState& state,const PoliticalMilitaryAppointmentCatalog* appointments,const PoliticalMilitaryAppointmentState* history){
    const bool enabled=appointments&&appointments->enabled;if(catalog.military_appointments!=enabled)return common_failure("PoliticalMilitaryDirectorProfileMismatch");const auto& policies=catalog.dynamic_subjects?catalog.policies:catalog.subjects;
    for(const auto& row:policies.rows()){const auto& policy=row.second;if(std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::military_appointment)==policy.templates.end())continue;if(!enabled||!catalog.dynamic_subjects)return common_failure("PoliticalMilitaryDirectorProfileRequired",row.first);bool matched=false;for(const auto& p:appointments->policies.rows())if(p.second.organization_id==policy.organization_id)matched=true;if(!matched)return common_failure("PoliticalMilitaryDirectorOrganizationMismatch",row.first);}
    const auto check=[&](const auto& plans)->Status{for(const auto& row:plans.rows()){const auto& p=row.second;if(p.template_id!="military_appointment")continue;const auto* policy=catalog.dynamic_subjects?policies.find(p.policy_id):policies.find(p.subject.id);const auto* order=history?history->authorizations.find(p.goal_ref):nullptr;if(!enabled||!policy||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::military_appointment)==policy->templates.end()||!order||order->target.next_actor_id!=p.subject.id||order->target.organization_id!=policy->organization_id)return common_failure("PoliticalMilitaryDirectorOrderMismatch",row.first);if(p.phase==PlanPhase::completed){bool accepted=false;for(const auto& a:history->answers.rows())if(a.second.authorization_fact_id==order->id&&a.second.actor_id==p.subject.id&&a.second.accepted)accepted=true;if(!accepted)return common_failure("PoliticalMilitaryDirectorAnswerMissing",row.first);}}return common_success();};auto valid=check(state.plans);return valid.ok?check(state.archived_plans):valid;
}
}
