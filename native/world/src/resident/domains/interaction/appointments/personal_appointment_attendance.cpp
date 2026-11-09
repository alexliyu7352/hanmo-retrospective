#include "personal_appointment_time2.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"
// 赴约只是下一条正常行旅提案；窗口内留场不发Wait Action，避免把可交谈的本人永久占住。
#include "personal_appointment_attendance.hpp"
#include "personal_appointment_detail.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {namespace {
using Result=Outcome<std::optional<DecisionProposal>>;
// 复制的transition仅本提案可写，不动调用方旧计划与任何Common对象。
Result waiting(DecisionProposal proposal,const StableId& reason){auto& next=const_cast<DirectorTransition&>(*proposal.transition);proposal.command.reset();next.phase=PlanPhase::waiting;next.wait_reason=reason;return {true,std::move(proposal),{}};}
const PersonalAppointmentAttendancePolicy* policy(const PersonalAppointmentAttendanceCatalog& catalog,const StableId& id){for(const auto& p:catalog.policies)if(p.appointment_policy_id==id)return &p;return nullptr;}
}
Status validate_personal_appointment_attendance_sources(const PersonalAppointmentAttendanceRead& read,const PersonalAppointmentAttendanceCatalog& catalog){
 auto valid=validate_personal_appointment_attendance_catalog(catalog,read.catalog);if(!valid.ok)return valid;
 valid=validate_personal_appointments(read.common,read.interaction,read.interaction_catalog,read.catalog,read.state,nullptr,read.exact);if(!valid.ok)return valid;
 if(read.written_visits||read.written_catalog){if(!read.written_visits||!read.written_catalog)return common_failure("VisitAppointmentAttendanceAccessMissing",{},ErrorKind::unsupported_capability);const auto&w=*read.written_visits;if(&w.common!=&read.common||&w.interaction!=&read.interaction||&w.appointments!=&read.catalog||&w.original_attendance!=&catalog)return common_failure("AppointmentAttendanceCandidateMismatch",{},ErrorKind::internal);return validate_visit_appointment_attendance_sources(w,*read.written_catalog);}
 return common_success();
}
Outcome<std::optional<DecisionProposal>> propose_personal_appointment_attendance(const PersonalAppointmentAttendanceRead& read,const PersonalAppointmentAttendanceCatalog& catalog,const TravelCatalog& travel,const KnownSubjectView& view,const DecisionProposal& original){
    const auto& c=read.common;auto valid=validate_personal_appointment_attendance_sources(read,catalog);if(!valid.ok)return {false,{},valid.error};
    if(!original.transition||original.subject_id!=view.actor_id||view.subject.kind!=SubjectKind::actor||view.subject.id!=view.actor_id||view.tick!=c.tick()||view.revision!=c.revision()||original.transition->expected_revision!=c.revision())return {false,{},common_failure("InvalidAppointmentAttendanceSource").error};
    if(read.exact){auto now=read.exact->clock.read_current_moment();if(!now.ok)return {false,{},now.error};if(now.value.half_day_tick!=c.tick())return {false,{},common_failure("StaleAppointmentProposalClock",view.actor_id,ErrorKind::internal).error};}
    if(view.actor_id==c.state().metadata.player_actor_id)return {true,std::nullopt,{}};
    const PersonalAppointmentAccepted* selected=nullptr;const auto known=read.state.by_participant.find(view.actor_id);
    if(known!=read.state.by_participant.end())for(const auto& id:known->second){const auto* a=read.state.accepted.find(id);if(!a)return {false,{},common_failure("PersonalAppointmentIndexMismatch",id).error};const auto* configured=policy(catalog,a->policy_id);
        auto status=read.exact?read_personal_appointment_status2(*read.exact,id):Outcome<StableId>{true,appointment_detail::status(read.state,*a,c.tick()),{}};if(!status.ok)return {false,{},status.error};
        // 先只承担原provider的亲自到场，不替另一方接受、交谈或开始双方行动。
        if(!configured||a->contact.provider_actor_id!=view.actor_id||status.value!="accepted"||c.tick()<std::max<Integer>(0,a->earliest_tick-configured->depart_before_ticks))continue;
        if(!selected||a->earliest_tick<selected->earliest_tick||(a->earliest_tick==selected->earliest_tick&&a->fact_id<selected->fact_id))selected=a;
    }
    // 新旧承诺均用本人原冻结开窗排序；任何另一端未知收取/答受都不参与选择。
    if(read.written_visits||read.written_catalog){if(!read.written_visits||!read.written_catalog)return {false,{},common_failure("VisitAppointmentAttendanceAccessMissing").error};auto written=propose_visit_appointment_attendance(*read.written_visits,*read.written_catalog,travel,view,original);if(!written.ok)return written;if(written.value){const auto* promise=read.written_visits->state.offered.find(written.value->transition->goal_ref);if(!promise)return {false,{},common_failure("VisitAppointmentAttendanceSourceRequired").error};if(!selected||promise->terms.earliest_tick<selected->earliest_tick)return written;}}
    if(!selected)return {true,std::nullopt,{}};
    DecisionProposal proposal=original;proposal.transition=checkpoint_make_shared<DirectorTransition>(*original.transition);auto& next=const_cast<DirectorTransition&>(*proposal.transition);proposal.command.reset();proposal.reason_fact_id=selected->fact_id;next.template_id=personal_appointment_attendance_template;next.goal_ref=selected->fact_id;next.commitment_ids={selected->fact_id};next.current_action_id.reset();next.next_step_index=0;next.wait_reason.clear();if(c.tick()==INT64_MAX)return {false,{},common_failure("AppointmentAttendanceTickOverflow").error};next.next_review_tick=std::min(next.next_review_tick,c.tick()+1);
    const auto* person=c.actor(view.actor_id);if(!person||!person->alive||!person->available||!view.alive||!view.available)return waiting(std::move(proposal),"AppointmentActorUnavailable");
    if(!person->action_id.empty()||!view.current_action_id.empty()){if(!person->action_id.empty())next.current_action_id=person->action_id;return waiting(std::move(proposal),"AppointmentExistingAction");}
    bool interaction_busy=view.interaction_busy;for(const auto& session:read.interaction.interactions)if(session.actor_id==view.actor_id||session.context.provider_actor_id==view.actor_id)interaction_busy=true;
    if(interaction_busy)return waiting(std::move(proposal),"AppointmentExistingInteraction");
    const auto* members=c.members(view.actor_id);if(!members||members->size()!=1||members->front()!=view.actor_id||!std::holds_alternative<SettlementLocation>(person->location))return waiting(std::move(proposal),"AppointmentIndependentTravelRequired");
    auto location=c.resolved_location(view.actor_id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||here->place_id!=view.place_id||here->settlement_id!=view.settlement_id)return {false,{},common_failure("AppointmentKnownLocationMismatch",view.actor_id).error};
    if(view.place_id==selected->contact.place_id)return waiting(std::move(proposal),c.tick()<selected->earliest_tick?"AppointmentWindowNotOpen":"AppointmentAwaitingOriginalParticipant");
    proposal.command=director_move_toward(travel,view,selected->contact.place_id);if(!proposal.command)return waiting(std::move(proposal),"AppointmentRouteUnavailable");next.phase=PlanPhase::delivering;next.next_step_index=1;return {true,std::move(proposal),{}};
}
// 时间唤醒来自本人实际既有承诺，不提高候选数量预算，也不替失效登记重办准入。
Status wake_personal_appointment_attendance(const PersonalAppointmentAttendanceRead& read,const PersonalAppointmentAttendanceCatalog& catalog,const DirectorCatalog& director,DirectorState& state){
    if(state.subject2)return common_failure("TypedAppointmentWakeOwnerRequired",{},ErrorKind::unsupported_capability);
    auto valid=validate_personal_appointment_attendance_catalog(catalog,read.catalog);if(!valid.ok)return valid;valid=validate_personal_appointments(read.common,read.interaction,read.interaction_catalog,read.catalog,read.state);if(!valid.ok)return valid;const auto tick=read.common.tick();
    for(const auto& row:read.state.accepted.rows()){const auto& a=row.second;const auto* terms=policy(catalog,a.policy_id);const auto& actor=a.contact.provider_actor_id;if(!terms||actor==read.common.state().metadata.player_actor_id||appointment_detail::status(read.state,a,tick)!="accepted"||!director_subject_active(state,actor))continue;
        const auto* behavior=director_policy(state,director,actor);auto* schedule=state.schedules.find(actor);if(!behavior||!schedule||std::none_of(behavior->templates.begin(),behavior->templates.end(),[](DirectorTemplate t){return static_cast<Integer>(t)==personal_appointment_attendance_template_number;}))continue;
        auto next=std::max(tick,std::max<Integer>(0,a.earliest_tick-terms->depart_before_ticks));if(schedule->last_decision_tick>=tick){if(tick==INT64_MAX)return common_failure("AppointmentAttendanceTickOverflow");next=std::max(next,tick+1);}if(next>a.latest_tick||next>=schedule->next_tick)continue;
        schedule->next_tick=next;if(auto* plan=state.plans.find(actor))plan->next_review_tick=next;
        if(std::find(schedule->wake_reasons.begin(),schedule->wake_reasons.end(),"PersonalAppointmentCommitment")==schedule->wake_reasons.end())schedule->wake_reasons.push_back("PersonalAppointmentCommitment");
    }return common_success();
}
Status validate_personal_appointment_attendance_plan(const CommonStore& common,const PersonalAppointmentState& state,const PersonalAppointmentAttendanceCatalog* catalog,const FinitePlan& plan){
    const StableId prefix=StableId{personal_appointment_accepted_kind}+":";if(plan.goal_ref.rfind(prefix,0)!=0)return plan.template_id==personal_appointment_attendance_template?common_failure("InvalidAppointmentAttendancePlan",plan.subject.id):common_success();
    const auto* source=state.accepted.find(plan.goal_ref);const auto* head=common.state().facts.find(plan.goal_ref);
    if(!catalog||catalog->version!=1||!source||!head||head->id!=source->fact_id||head->kind!=personal_appointment_accepted_kind||head->domain!=personal_appointment_module||!policy(*catalog,source->policy_id)||plan.template_id!=personal_appointment_attendance_template||plan.subject.kind!=SubjectKind::actor||source->contact.provider_actor_id!=plan.subject.id||plan.commitment_ids!=std::vector<StableId>{source->fact_id}||plan.step_index<0||plan.step_index>1)return common_failure("InvalidAppointmentAttendancePlan",plan.subject.id);
    return common_success();
}
}
