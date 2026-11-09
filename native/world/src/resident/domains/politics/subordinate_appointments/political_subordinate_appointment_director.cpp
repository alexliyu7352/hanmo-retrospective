#include "political_subordinate_appointment_director.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_subordinate_appointment(const CommonStore& c,const KnownSubjectView& known,const DirectorPolicy& policy,const StableId& selected,const PoliticalSubordinateAppointmentDirectorRead& read,const DecisionProposal& base,Integer budget){
 if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor||known.subject.id!=known.actor_id||known.tick!=c.tick()||known.revision!=c.revision()||base.transition->expected_revision!=c.revision()||budget<1||budget>64)return {false,{},common_failure("InvalidPoliticalSubordinateDirectorProposal",known.actor_id,ErrorKind::internal).error};
 if(!policy.political_career||std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::political_career)==policy.templates.end())return {true,std::nullopt,{}};
 if(base.command||base.transition->current_action_id||!known.commitments.empty()||!base.transition->commitment_ids.empty()||base.transition->template_id=="maintain_commitment"||known.actor_id==c.state().metadata.player_actor_id||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)return {true,std::nullopt,{}};
 auto choice=read.choose(known.actor_id,selected,budget);if(!choice.ok)return {false,{},choice.error};const auto& k=choice.value;if(k.intent_id.empty()||(!k.command&&!k.travel&&k.source_fact_id.empty()&&k.reason=="NoPoliticalSubordinateOpportunity"))return {true,std::nullopt,{}};
 if(k.completed&&base.transition->template_id==political_subordinate_appointment_template&&base.transition->phase==PlanPhase::completed&&base.transition->goal_ref==k.source_fact_id)return {true,std::nullopt,{}};
 auto out=base;auto transition=std::make_shared<DirectorTransition>(*base.transition);out.transition=transition;auto& next=*transition;next.template_id=political_subordinate_appointment_template;next.goal_ref=k.source_fact_id.empty()?k.intent_id:k.source_fact_id;next.phase=PlanPhase::waiting;next.current_action_id.reset();next.next_step_index=0;next.wait_reason.clear();next.livelihood.reset();next.pending_life_action_id.reset();next.last_life_fact_id.reset();out.reason_fact_id=k.source_fact_id;
 // 本次命令成功只留待复评；不能在事实尚未分配时预告完成或猜下一个实例ID。
 if(k.command){auto command=read.command(*k.command);if(!command.ok)return {false,{},command.error};if(!command.value||command.value->kind!=political_subordinate_appointment_command_kind(k.command->command))return {false,{},common_failure("PoliticalSubordinateDirectorCommandRequired",k.intent_id,ErrorKind::internal).error};out.command=std::move(command.value);}
 else if(k.travel&&k.travel->command){std::visit([&](const auto& cmd){using T=std::decay_t<decltype(cmd)>;out.command=std::make_shared<const CommandPayload>(CommandPayload{std::is_same_v<T,MoveLocalCommand>?"move_local":"travel",TravelCommandPayload{cmd}});},*k.travel->command);next.phase=PlanPhase::delivering;}
 else if(k.completed){if(k.source_fact_id.empty())return {false,{},common_failure("PoliticalSubordinateCompletedSourceRequired",k.intent_id,ErrorKind::internal).error};next.phase=PlanPhase::completed;}
 else next.wait_reason=k.reason.empty()?StableId{"PoliticalSubordinateAppointmentWaiting"}:k.reason;
 return {true,std::move(out),{}};
}
}
