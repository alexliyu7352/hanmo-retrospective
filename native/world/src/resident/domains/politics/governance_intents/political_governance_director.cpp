// 当面签发与本人受领分别提交原命令；作者意图不是政治授权，也不替代真实足额钱粮预览。
#include "political_governance_director.hpp"
#include "resident/kernel/governance_integration.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/domains/director/director_livelihood.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {namespace {
// 只有原双方亲自签发、原署和原计划的邀请可满足一次意图；代理实际签字另有来源，不冒首长的自动意图完成。
bool matches(const GovernanceAuthorityRecord& r,const PoliticalGovernanceIntent& i){return r.id.rfind("politics_governance_authority:",0)==0&&r.issuer_actor_id==i.issuer_actor_id&&r.executor_actor_id==i.executor_actor_id&&r.office_id==i.office_id&&r.jurisdiction_id==i.jurisdiction_id&&r.policy_id==i.commission_policy_id&&r.public_inventory_id==i.public_inventory_id;}
// 从真实Common顺序选择首次具名邀请；后来手工再签不会恢复已消耗的自动签发次数。
Outcome<const GovernanceAuthorityRecord*> first_source(const GovernanceReadContext& c,const PoliticalGovernanceIntent& i){
 PoliticsGovernanceAccess access(c.common,c.politics,c.governance,c.governance_catalog,c.organization,c.organization_catalog,c.activity,c.agencies);const GovernanceAuthorityRecord* selected=nullptr;std::size_t found=0,expected=0;
 for(const auto& row:c.governance.authorities.rows())if(matches(row.second,i)){++expected;auto read=access.governance_authority_approval(row.first);if(!read.ok)return {false,nullptr,read.error};}
 for(const auto& head:c.common.state().facts.rows()){const auto* r=c.governance.authorities.find(head.first);if(r&&matches(*r,i)){if(!selected)selected=r;++found;}}
 if(found!=expected)return {false,nullptr,common_failure("GovernanceIntentHistoryUnavailable",i.id).error};return {true,selected,{}};
}
// 本人已有Task事实是接受完成，不依赖今天是否仍工作、已结酬或被取消。
Outcome<std::optional<GovernanceTaskAcceptance>> acceptance(const GovernanceReadContext& c,const GovernanceAuthorityRecord& a){
 GovernanceTaskRead tasks(c.common,c.tasks,c.governance_tasks);std::optional<GovernanceTaskAcceptance> out;
 for(const auto& row:c.governance_tasks.governance_contracts.rows())if(row.second.acceptance.authority_source_fact_id==a.id){auto read=tasks.governance_task_source(row.first);if(!read.ok)return {false,{},read.error};const auto& v=read.value.acceptance;const auto* head=c.common.state().facts.find(v.fact_id);if(out||v.executor_actor_id!=a.executor_actor_id||v.organization_id!=a.organization_id||v.jurisdiction_id!=a.jurisdiction_id||v.site_id!=a.site_id||!head||head->id!=v.fact_id||head->domain!="tasks"||head->kind!="governance_task_accepted")return {false,{},common_failure("GovernanceIntentAcceptanceMismatch",row.first).error};out=v;}
 return {true,std::move(out),{}};
}
}
// 原责任、当前行动与会话均先行；远方仅用本人原邀请/受领史及作者治所，到场才核实时双方资格。
Outcome<std::optional<DecisionProposal>> propose_director_governance_commission(const GovernanceReadContext& c,const TravelCatalog& travel,const KnownSubjectView& known,const DirectorPolicy& policy,const PoliticalGovernanceIntent& i,const DecisionProposal& base,Integer limit){
 if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor||known.subject.id!=known.actor_id||known.tick!=c.common.tick()||known.revision!=c.common.revision()||base.transition->expected_revision!=known.revision||limit<1||limit>64)return {false,{},common_failure("InvalidGovernanceIntentProposal",known.actor_id,ErrorKind::internal).error};
 if(base.command||base.transition->current_action_id||!known.commitments.empty()||!base.transition->commitment_ids.empty()||base.transition->template_id=="maintain_commitment"||known.actor_id==c.common.state().metadata.player_actor_id||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)return {true,std::nullopt,{}};
 const bool issuer=known.actor_id==i.issuer_actor_id,executor=known.actor_id==i.executor_actor_id;if(!issuer&&!executor)return {true,std::nullopt,{}};
 const auto* j=c.politics.jurisdictions.find(i.jurisdiction_id);const auto* actor=c.common.actor(known.actor_id);const auto* office=c.organization.offices.find(i.office_id);
 if(!j||!actor||!office||policy.organization_id!=j->political_organization_id)return {false,{},common_failure("GovernanceIntentSourceMismatch",i.id).error};if(!std::holds_alternative<SettlementLocation>(actor->location))return {true,std::nullopt,{}};
 if(issuer&&(office->holder_actor_id!=std::optional<StableId>{known.actor_id}||office->status!=OfficeStatus::active))return {true,std::nullopt,{}};
 OrganizationAccess organizations(c.common,c.organization,c.organization_catalog,c.economy);if(!organizations.actor_accepts_duty(known.actor_id,policy.organization_id))return {true,std::nullopt,{}};
 auto out=base;auto next=std::make_shared<DirectorTransition>(*base.transition);out.transition=next;next->template_id=political_governance_intent_template;next->goal_ref=i.id;next->current_action_id.reset();next->next_step_index=0;next->wait_reason.clear();next->livelihood.reset();next->pending_life_action_id.reset();next->last_life_fact_id.reset();out.reason_fact_id.clear();
 // 原签发只能在双方同场形成，因此两人可永久记得这份旧邀请；不据此读取对方今日位置。
 auto source=first_source(c,i);if(!source.ok)return {false,{},source.error};std::optional<GovernanceTaskAcceptance> accepted;
 if(executor&&source.value){auto read=acceptance(c,*source.value);if(!read.ok)return {false,{},read.error};accepted=std::move(read.value);}
 const StableId completed=issuer&&source.value?source.value->id:accepted?accepted->fact_id:StableId{};
 if(!completed.empty()){if(base.transition->template_id==political_governance_intent_template&&base.transition->phase==PlanPhase::completed&&base.transition->goal_ref==completed)return {true,std::nullopt,{}};next->phase=PlanPhase::completed;next->goal_ref=completed;out.reason_fact_id=completed;return {true,std::move(out),{}};}
 if(executor&&source.value&&c.common.tick()>=source.value->offer_expires_tick){next->phase=PlanPhase::waiting;next->wait_reason="GovernanceIntentOriginalOfferExpired";out.reason_fact_id=source.value->id;return {true,std::move(out),{}};}
 if(known.place_id!=j->seat_place_id){out.command=director_move_toward(travel,known,j->seat_place_id);next->phase=out.command?PlanPhase::delivering:PlanPhase::waiting;next->wait_reason=out.command?StableId{}:"GovernanceIntentRouteUnavailable";return {true,std::move(out),{}};}
 next->phase=PlanPhase::waiting;
 if(issuer){
  GovernanceCommand command=PoliticalGovernanceCommissionCommand{{office->organization_id,office->id,office->epoch,known.actor_id},i.executor_actor_id,j->id,i.commission_policy_id,i.public_inventory_id,j->revision};auto preview=preview_governance_business(c,known.actor_id,command);if(!preview.ok){if(preview.error.kind!=ErrorKind::rule)return {false,{},preview.error};next->wait_reason=preview.error.original.code;return {true,std::move(out),{}};}
  out.command=std::make_shared<const CommandPayload>(CommandPayload{"political_commission_governance",std::move(command)});next->phase=PlanPhase::accepting;next->wait_reason.clear();return {true,std::move(out),{}};
 }
 if(!source.value){next->wait_reason="GovernanceIntentAwaitingLocalCommission";return {true,std::move(out),{}};}
 GovernanceCommand command=GovernanceAcceptCommand{source.value->id,c.common.revision()};auto preview=preview_governance_business(c,known.actor_id,command);if(!preview.ok){if(preview.error.kind!=ErrorKind::rule)return {false,{},preview.error};next->wait_reason=preview.error.original.code;out.reason_fact_id=source.value->id;return {true,std::move(out),{}};}
 out.command=std::make_shared<const CommandPayload>(CommandPayload{"governance_accept",std::move(command)});out.reason_fact_id=source.value->id;next->phase=PlanPhase::accepting;next->wait_reason.clear();return {true,std::move(out),{}};
}
// 必须已有实际业务事实且不晚于原复评；不把提案本身、任意fact或后来新源当完成。
Status validate_governance_intent_completed_source(const GovernanceReadContext& c,const PoliticalGovernanceIntent& i,const StableId& actor,const StableId& fact,Integer tick){
 if(tick<0||tick>c.common.tick())return common_failure("GovernanceIntentCompletedSourceMismatch",fact);auto first=first_source(c,i);if(!first.ok)return {false,{},first.error};if(!first.value||first.value->issued_tick>tick)return common_failure("GovernanceIntentCompletedSourceMismatch",fact);
 if(actor==i.issuer_actor_id)return fact==first.value->id?common_success():common_failure("GovernanceIntentCompletedSourceMismatch",fact);
 if(actor!=i.executor_actor_id)return common_failure("GovernanceIntentCompletedSourceMismatch",fact);auto accepted=acceptance(c,*first.value);if(!accepted.ok)return {false,{},accepted.error};return accepted.value&&accepted.value->fact_id==fact&&accepted.value->accepted_tick<=tick?common_success():common_failure("GovernanceIntentCompletedSourceMismatch",fact);
}
}
