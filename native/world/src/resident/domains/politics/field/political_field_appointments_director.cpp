// 只有作者显式野外专项策略会面授、接受或交接；读取范围始终限真实本军与本人军令。
#include "political_field_appointments_director.hpp"
#include "political_field_appointments_detail.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_field_appointment(const PoliticalFieldAppointmentContext& c,const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){
 if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor||known.subject.id!=known.actor_id||known.tick!=c.common.tick()||known.revision!=c.common.revision()||base.transition->expected_revision!=known.revision)return {false,{},common_failure("InvalidFieldDirectorProposal",known.actor_id,ErrorKind::internal).error};
 if(known.actor_id==c.common.state().metadata.player_actor_id||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)return {true,std::nullopt,{}};
 auto audit=political_field_detail::audit(c);if(!audit.ok)return {false,{},audit.error};
 // 提案复制原调度变更，不能另给预算、重置公平序或直接写入领域记录。
 auto proposal=[&](const StableId& goal,const StableId& reason,const StableId& kind,auto command){auto out=base;auto next=std::make_shared<DirectorTransition>(*base.transition);out.transition=next;next->template_id="field_appointment";next->goal_ref=goal;next->current_action_id.reset();next->next_step_index=0;next->phase=PlanPhase::completed;next->wait_reason.clear();out.reason_fact_id=reason;out.command=std::make_shared<const CommandPayload>(CommandPayload{kind,std::move(command)});return out;};
 Integer scanned=0;
 for(const auto& row:c.state.authorizations.rows()){
  const auto& order=row.second;if(order.target.next_actor_id!=known.actor_id||order.target.organization_id!=policy.organization_id||audit.value.withdrawals.count(order.id))continue;if(++scanned>256)break;
  const auto answer=audit.value.answers.find(order.id);
  if(answer==audit.value.answers.end()){
   if(c.common.tick()>=order.answer_before_tick)continue;PoliticalFieldAppointmentCommand command=PoliticalFieldAnswerCommand{order.id,true};auto quote=preview_political_field_appointment(c,known.actor_id,command);if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};continue;}
   return {true,proposal(order.id,order.id,"political_field_appointment_answer",std::move(command)),{}};
  }
  if(!answer->second->accepted||c.common.tick()>=answer->second->apply_before_tick)continue;
  auto authority=PoliticalFieldAppointmentAccess(c).appointment_for_application(order.id,answer->second->id,known.actor_id);if(!authority.ok){if(authority.error.kind!=ErrorKind::rule)return {false,{},authority.error};continue;}
  if(!c.military.application||!c.military.application_preview)return {false,{},common_failure("PoliticalFieldApplicationReadMissing",{},ErrorKind::internal).error};auto applied=c.military.application(order.id);if(!applied.ok)return {false,{},applied.error};if(applied.value)continue;
  auto actual=c.military.application_preview(known.actor_id,order.id,answer->second->id);if(!actual.ok){if(actual.error.kind!=ErrorKind::rule)return {false,{},actual.error};continue;}
  MilitaryCommand command=MilitaryFieldCommandAssign{"field:"+order.id,order.id,answer->second->id,{}};
  return {true,proposal(order.id,answer->second->id,"military_field_command_assign",std::move(command)),{}};
 }
 if(!c.military.targets)return {false,{},common_failure("PoliticalFieldTargetReadMissing",{},ErrorKind::internal).error};auto targets=c.military.targets(known.actor_id);if(!targets.ok)return {false,{},targets.error};scanned=0;
 // 已有有效军令先等待本人答复；不在每次复评重签同一指挥来源。
 for(const auto& target:targets.value){if(++scanned>64)break;if(target.organization_id!=policy.organization_id)continue;bool pending=false;for(const auto& row:c.state.authorizations.rows()){const auto& old=row.second;if(old.target.kind!=target.kind||old.target.army_id!=target.army_id||old.target.unit_id!=target.unit_id||old.target.next_actor_id!=target.next_actor_id||!political_field_detail::same_source(old.target.previous_source,target.previous_source)||audit.value.withdrawals.count(old.id))continue;const auto answer=audit.value.answers.find(old.id);if(answer==audit.value.answers.end()?c.common.tick()<old.answer_before_tick:answer->second->accepted&&c.common.tick()<answer->second->apply_before_tick){pending=true;break;}}if(pending)continue;
  for(const auto& policy_row:c.catalog.policies.rows()){if(policy_row.second.organization_id!=target.organization_id)continue;for(const auto& row:c.organizations.offices.rows()){const auto& office=row.second;if(office.organization_id!=policy.organization_id||office.holder_actor_id!=std::optional<StableId>{known.actor_id})continue;PoliticalFieldAppointmentCommand command=PoliticalFieldIssueCommand{{office.organization_id,office.id,office.epoch,known.actor_id},policy_row.first,target};auto quote=preview_political_field_appointment(c,known.actor_id,command);if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};continue;}
   // 签发时以真实伤亡源作计划锚；采用后冷验必须找到本人由此源签出的真实军令。
   return {true,proposal(target.predecessor_death.casualty_fact_id,target.predecessor_death.casualty_fact_id,"political_field_appointment_issue",std::move(command)),{}};
  }}
 }
 return {true,std::nullopt,{}};
}
// 已完成计划必须有同主体、同组织的真实签受来源；计划本身从不授权军事交接。
Status validate_political_field_director(const DirectorCatalog& catalog,const DirectorState& state,const PoliticalFieldAppointmentCatalog* appointments,const PoliticalFieldAppointmentState* history){
 const bool enabled=appointments&&appointments->enabled;if(catalog.field_appointments!=enabled)return common_failure("PoliticalFieldDirectorProfileMismatch");const auto& policies=catalog.dynamic_subjects?catalog.policies:catalog.subjects;
 for(const auto& row:policies.rows()){if(std::find(row.second.templates.begin(),row.second.templates.end(),DirectorTemplate::field_appointment)==row.second.templates.end())continue;if(!enabled||!catalog.dynamic_subjects)return common_failure("PoliticalFieldDirectorProfileRequired",row.first);bool matched=false;for(const auto& p:appointments->policies.rows())if(p.second.organization_id==row.second.organization_id)matched=true;if(!matched)return common_failure("PoliticalFieldDirectorOrganizationMismatch",row.first);}
 auto check=[&](const auto& plans)->Status{for(const auto& row:plans.rows()){const auto& p=row.second;if(p.template_id!="field_appointment")continue;const auto* policy=catalog.dynamic_subjects?policies.find(p.policy_id):policies.find(p.subject.id);if(!enabled||!history||!policy||p.subject.kind!=SubjectKind::actor||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::field_appointment)==policy->templates.end())return common_failure("PoliticalFieldDirectorOrderMismatch",row.first);
  const auto* order=history->authorizations.find(p.goal_ref);if(order){if(order->target.next_actor_id!=p.subject.id||order->target.organization_id!=policy->organization_id)return common_failure("PoliticalFieldDirectorOrderMismatch",row.first);if(p.phase==PlanPhase::completed){bool accepted=false;for(const auto& a:history->answers.rows())if(a.second.authorization_fact_id==order->id&&a.second.actor_id==p.subject.id&&a.second.accepted)accepted=true;if(!accepted)return common_failure("PoliticalFieldDirectorAnswerMissing",row.first);}}
  else {bool issued=false;for(const auto& r:history->authorizations.rows())if(r.second.authority.actor_id==p.subject.id&&r.second.target.organization_id==policy->organization_id&&r.second.target.predecessor_death.casualty_fact_id==p.goal_ref)issued=true;if(!issued||p.phase!=PlanPhase::completed)return common_failure("PoliticalFieldDirectorIssueMissing",row.first);}
 }return common_success();};auto valid=check(state.plans);return valid.ok?check(state.archived_plans):valid;
}
}
