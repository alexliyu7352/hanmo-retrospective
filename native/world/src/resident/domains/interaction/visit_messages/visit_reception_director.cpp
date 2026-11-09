#include "resident/domains/interaction/visit_appointments/visit_appointment_director.hpp"
// 有界来访只驱动当前当地岗位的原命令；看不到远方收件状态，也不结束未认领的会话。
#include "visit_reception_director.hpp"
#include "resident/domains/director/director_actor_read.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_director.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/kernel/command_types.hpp"
#include "visit_message_life.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {namespace {
using Result=Outcome<std::optional<DecisionProposal>>;
const VisitMessagePolicy* source(const VisitReceptionRead& r,const VisitReceptionIntent& intent){for(const auto& p:r.message_catalog.policies)if(p.id==intent.message_policy_id)return &p;return nullptr;}
bool responsibility(const VisitReceptionRead& r,const StableId& actor){for(const auto& row:r.tasks.tasks.rows())if(row.second.assignee==actor&&(row.second.status=="accepted"||row.second.status=="active"||row.second.status=="in_progress"||row.second.status=="ready"||row.second.status=="settling"))return true;return false;}
const InteractionSession* session(const VisitReceptionRead& r,const StableId& actor){const InteractionSession* found=nullptr;for(const auto& s:r.interactions.interactions)if(s.actor_id==actor){if(found)return nullptr;found=&s;}return found;}
bool same_session(const InteractionSession& s,const VisitMessagePolicy& p,const StableId& actor){return s.actor_id==actor&&s.context.provider_actor_id==p.custodian_actor_id&&s.context.role_id==p.role_id&&!s.context.kind&&!s.context.encounter_id&&s.dialogue_id==p.dialogue_id;}
bool finished(const VisitReceptionRead& r,const VisitReceptionIntent& i){if(i.purpose=="deposit"){for(const auto& row:r.messages.posted.rows())if(row.second.sender_actor_id==i.actor_id&&row.second.policy_id==i.message_policy_id)return true;}else for(const auto& row:r.messages.received.rows())if(row.second.contact.actor_id==i.actor_id){const auto* m=r.messages.posted.find(row.second.message_fact_id);if(m&&m->policy_id==i.message_policy_id)return true;}return false;}
DirectorTransition& transition(DecisionProposal& p){return const_cast<DirectorTransition&>(*p.transition);}
Result wait(DecisionProposal p,const StableId& code){auto& n=transition(p);n.phase=PlanPhase::waiting;n.wait_reason=code;p.command.reset();return {true,std::move(p),{}};}
Result interaction(const VisitReceptionRead& r,DecisionProposal p,const StableId& kind,const InteractionCommand& command){auto preview=preview_interaction(r.common,r.interactions,r.interaction_catalog,r.access,p.subject_id,command);if(!preview.ok){if(preview.error.kind!=ErrorKind::rule)return {false,{},preview.error};return wait(std::move(p),preview.error.original.code);}transition(p).wait_reason.clear();p.command=std::make_shared<const CommandPayload>(CommandPayload{kind,command});return {true,std::move(p),{}};}
Result close(const VisitReceptionRead& r,const VisitReceptionIntent& i,DecisionProposal p){if(r.common.tick()>std::numeric_limits<Integer>::max()-i.cooldown_ticks)return {false,{},common_failure("VisitReceptionTickOverflow",i.id).error};auto& n=transition(p);n.phase=PlanPhase::completed;n.next_step_index=0;n.next_review_tick=std::max(n.next_review_tick,r.common.tick()+i.cooldown_ticks);return interaction(r,std::move(p),"end_interaction",EndInteraction{});}
bool session_id(const CommonStore& common,const StableId& id){const StableId prefix="interaction:";if(id.rfind(prefix,0)!=0)return false;auto digits=id.substr(prefix.size());Integer number=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),number);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&number>0&&number<common.state().next_instance_id&&std::to_string(number)==digits;}
}
Outcome<bool> visit_reception_conversation_ready(const VisitReceptionRead& r,const DirectorState& director,const StableId& actor,const StableId& intent_id){
    const auto* i=visit_reception_intent(r.catalog,intent_id);const auto* p=i?source(r,*i):nullptr;auto selected_plan=read_director_actor_plan(director,actor);if(!selected_plan.ok)return {false,{},selected_plan.error};const auto* plan=selected_plan.value;const auto* s=session(r,actor);const auto* person=r.common.actor(actor);
    if(!i||i->actor_id!=actor||!p)return {false,{},common_failure("InvalidVisitReceptionIntent",intent_id).error};
    return {true,person&&person->alive&&person->available&&person->action_id.empty()&&!responsibility(r,actor)&&plan&&!plan->pending_life_action_id&&plan->template_id=="visit_reception"&&s&&plan->goal_ref==s->id&&same_session(*s,*p,actor)&&read_visit_reception_claim(r.claims,s->id,actor,intent_id),{}};
}
Result propose_visit_reception(const VisitReceptionRead& r,const DirectorState& director,const KnownSubjectView& view,const StableId& intent_id,const DecisionProposal& original){
    const auto* i=visit_reception_intent(r.catalog,intent_id);const auto* p=i?source(r,*i):nullptr;
    if(!i||i->actor_id!=view.actor_id||!p||!original.transition||original.subject_id!=view.actor_id||view.subject.kind!=SubjectKind::actor||view.subject.id!=view.actor_id||view.tick!=r.common.tick()||view.revision!=r.common.revision()||original.transition->expected_revision!=r.common.revision())return {false,{},common_failure("InvalidVisitReceptionInput",intent_id).error};
    if(!view.alive||!view.available||!view.current_action_id.empty()||original.transition->pending_life_action_id||responsibility(r,view.actor_id))return {true,std::nullopt,{}};
    const auto* role=r.interaction_catalog.common->roles.find(p->role_id);const auto* actor=r.common.actor(view.actor_id);if(!role||!actor)return {false,{},common_failure("InvalidVisitReceptionSource",intent_id).error};
    // 到场之前不读接待人/收件人现况，也不查询未获知的待收拜帖；仅有本地作者routine。
    auto location=r.common.resolved_location(view.actor_id);const auto* here=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!here||here->place_id!=view.place_id||view.place_id!=role->place_id||!actor->action_id.empty()||std::holds_alternative<FollowingLocation>(actor->location))return {true,std::nullopt,{}};
    const auto* s=session(r,view.actor_id);auto selected_plan=read_director_actor_plan(director,view.actor_id);if(!selected_plan.ok)return {false,{},selected_plan.error};const auto* old=selected_plan.value;const bool claimed=old&&old->template_id=="visit_reception"&&s&&old->goal_ref==s->id&&same_session(*s,*p,view.actor_id)&&read_visit_reception_claim(r.claims,s->id,view.actor_id,intent_id);
    if(view.interaction_busy&&!claimed)return {true,std::nullopt,{}};if(s&&!claimed)return {true,std::nullopt,{}};
    DecisionProposal proposal=original;proposal.transition=std::make_shared<DirectorTransition>(*original.transition);auto& next=transition(proposal);next.template_id="visit_reception";next.current_action_id.reset();next.commitment_ids.clear();next.wait_reason.clear();proposal.command.reset();
    if(!s){bool reply_pending=false;if(r.replies){auto pending=visit_reply_reception_pending(r,*r.replies,view.actor_id,intent_id);if(!pending.ok)return {false,{},pending.error};reply_pending=pending.value;}if(r.invitations){auto pending=visit_appointment_reception_pending(r,*r.invitations,view.actor_id,intent_id);if(!pending.ok)return {false,{},pending.error};reply_pending=reply_pending||pending.value;}if(finished(r,*i)&&!reply_pending)return {true,std::nullopt,{}};const auto* assignment=r.common.state().roles.find(p->role_id);if(!assignment||!assignment->enabled||assignment->actor_id!=p->custodian_actor_id)return {true,std::nullopt,{}};next.goal_ref=i->id;next.phase=PlanPhase::selecting;next.next_step_index=0;return interaction(r,std::move(proposal),"talk",Talk{p->role_id,{}});}
    next.goal_ref=s->id;
    if(r.replies){auto answer=propose_received_visit_reply(r,*r.replies,director,view,intent_id,proposal);if(!answer.ok||answer.value)return answer;}
    if(r.invitations){auto answer=propose_received_visit_appointment(r,*r.invitations,director,view,intent_id,proposal);if(!answer.ok)return {false,{},answer.error};if(answer.value){auto step=std::move(*answer.value);std::visit([&](const auto& request){step.proposal.command=std::make_shared<const CommandPayload>(CommandPayload{step.command_kind,request});},step.command);return {true,std::move(step.proposal),{}};}}
    if(old->phase==PlanPhase::completed||finished(r,*i))return close(r,*i,std::move(proposal));
    const auto target=i->purpose=="deposit"?p->deposit_node_id:p->receive_node_id;
    if(s->node_id!=target){for(std::size_t at=0;at<i->route.size();++at)if(i->route[at].node_id==s->node_id){const auto& step=i->route[at];next.phase=PlanPhase::accepting;next.next_step_index=static_cast<Integer>(at+1);return step.option_id.empty()?interaction(r,std::move(proposal),"dialogue_next",DialogueNext{}):interaction(r,std::move(proposal),"dialogue_choose",DialogueChoose{step.option_id});}return close(r,*i,std::move(proposal));}
    // 只有实际到了接待人原领读声明才枚举本人的待收源；托留也重验真实原speaker。
    auto spoken=preview_authored_provider_source(r.common,r.interactions,r.interaction_catalog,r.access,view.actor_id,p->custodian_actor_id);if(!spoken.ok){if(spoken.error.kind!=ErrorKind::rule)return {false,{},spoken.error};return close(r,*i,std::move(proposal));}
    VisitMessageCommand command=LeaveVisitMessage{p->id};
    if(i->purpose=="receive"){StableId pending;for(const auto& row:r.messages.posted.rows())if(row.second.policy_id==p->id&&row.second.recipient_actor_id==view.actor_id){bool ended=false;for(const auto* table:{&r.messages.received,&r.messages.withdrawn})for(const auto& end:table->rows())ended=ended||end.second.message_fact_id==row.first;if(!ended){if(!pending.empty())return {false,{},common_failure("InvalidVisitMessageDeposit",p->id).error};pending=row.first;}}if(pending.empty())return close(r,*i,std::move(proposal));command=ReceiveVisitMessage{pending};}
    auto prepared=preview_visit_message(r.common,r.messages,r.message_catalog,r.interactions,r.interaction_catalog,r.access,view.actor_id,command,r.message_life);if(!prepared.ok){if(prepared.error.kind!=ErrorKind::rule)return {false,{},prepared.error};return close(r,*i,std::move(proposal));}
    next.phase=PlanPhase::completed;next.next_step_index=static_cast<Integer>(i->route.size()+1);proposal.command=std::make_shared<const CommandPayload>(CommandPayload{visit_message_command_kind(command),command});return {true,std::move(proposal),{}};
}
Outcome<DecisionProposal> bind_visit_reception_adopted(const VisitReceptionRead& r,const StableId& intent_id,const DecisionProposal& original){
    if(!original.transition||original.transition->template_id!="visit_reception"||!original.command||(original.command->kind!="talk"&&original.command->kind!="end_interaction"))return {true,original,{}};
    if(original.command->kind=="end_interaction"){const auto* i=visit_reception_intent(r.catalog,intent_id);const auto* command=std::get_if<InteractionCommand>(&original.command->data);if(!i||i->actor_id!=original.subject_id||!command||!std::holds_alternative<EndInteraction>(*command)||!session_id(r.common,original.transition->goal_ref)||session(r,original.subject_id))return {false,{},common_failure("VisitReceptionEndSourceMismatch",intent_id,ErrorKind::internal).error};DecisionProposal result=original;result.transition=std::make_shared<DirectorTransition>(*original.transition);transition(result).goal_ref.clear();return {true,std::move(result),{}};}
    const auto* i=visit_reception_intent(r.catalog,intent_id);const auto* p=i?source(r,*i):nullptr;const auto* command=std::get_if<InteractionCommand>(&original.command->data);const auto* talk=command?std::get_if<Talk>(command):nullptr;const auto* s=session(r,original.subject_id);
    if(!i||i->actor_id!=original.subject_id||!p||!talk||talk->role_id!=p->role_id||!talk->topic_id.empty()||original.transition->goal_ref!=i->id||!s||!session_id(r.common,s->id)||!same_session(*s,*p,original.subject_id))return {false,{},common_failure("VisitReceptionTalkSourceMismatch",intent_id,ErrorKind::internal).error};
    DecisionProposal result=original;result.transition=std::make_shared<DirectorTransition>(*original.transition);auto& next=transition(result);next.goal_ref=s->id;next.phase=PlanPhase::accepting;return {true,std::move(result),{}};
}
Status validate_visit_reception_plan(const VisitReceptionRead& r,const FinitePlan& plan,const StableId& intent_id){
    if(plan.template_id!="visit_reception")return common_success();const auto* i=visit_reception_intent(r.catalog,intent_id);if(!i||i->actor_id!=plan.subject.id||plan.subject.kind!=SubjectKind::actor||plan.current_action_id||plan.pending_life_action_id||!plan.commitment_ids.empty()||plan.step_index<0||plan.step_index>static_cast<Integer>(i->route.size()+1))return common_failure("InvalidVisitReceptionPlan",plan.id);
    if(plan.goal_ref.empty())return plan.phase==PlanPhase::completed||plan.phase==PlanPhase::waiting?common_success():common_failure("InvalidVisitReceptionPlan",plan.id);
    if(plan.goal_ref==i->id)return plan.phase==PlanPhase::selecting||plan.phase==PlanPhase::waiting?common_success():common_failure("InvalidVisitReceptionPlan",plan.id);
    // 旧session后来合法结束或人物改由玩家控制不改写历史计划；下一提案只认当前完全同ID会话。
    if(!session_id(r.common,plan.goal_ref)||!read_visit_reception_claim(r.claims,plan.goal_ref,plan.subject.id,intent_id))return common_failure("InvalidVisitReceptionSession",plan.id);
    const auto* p=source(r,*i);for(const auto& s:r.interactions.interactions)if(s.id==plan.goal_ref&&(!p||!same_session(s,*p,plan.subject.id)))return common_failure("VisitReceptionSessionSourceMismatch",plan.id);return common_success();
}
}
