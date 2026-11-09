#include "resident/domains/organization/office_authority.hpp"
// 续办历史逐原Task、接管决定及当时签字权限复核；今日职位与Owner不能倒填旧许可。
#include "political_governance_continuation_detail.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include <set>
#include <limits>
namespace hanmo::resident::governance_continue_detail {
// 分配顺序与真实Common插入序必须相同，防同tick调换旧事实。
bool before(const CommonStore& c,const StableId& a,const StableId& b){bool seen=false;for(const auto& row:c.state().facts.rows()){if(row.first==b)return seen;if(row.first==a)seen=true;}return false;}
Outcome<Integer> source_sequence(const CommonStore& c,const StableId& id){auto n=organization_instance_sequence(id,political_governance_continue_prefix);const auto* h=c.state().facts.find(id);if(!n||*n>=c.state().next_instance_id||!h||h->id!=id||h->kind!=political_governance_continue_kind||h->domain!=political_governance_continuation_module)return {false,0,common_failure("GovernanceContinuationFactMismatch",id).error};return {true,*n,{}};}
// phase只让既有军史观察下一半日，续办签发仍为已经提交的Common时刻。
Status phase(const PoliticalGovernanceContinuationContext& x){if(x.common.tick()<0||x.state.version!=1)return common_failure("InvalidGovernanceContinuationState");if(x.phase&&(x.common.tick()==std::numeric_limits<Integer>::max()||x.phase->from_tick!=x.common.tick()||x.phase->to_tick!=x.common.tick()+1))return common_failure("GovernanceContinuationPhaseMismatch");return common_success();}
// 使用真实军事死因顺序，不把Family晚登记以前视作仍能签字。
Status alive_before(const PoliticalGovernanceContinuationContext& x,const StableId& id,Integer tick,const StableId& boundary){
 const auto* actor=x.common.actor(id);if(!actor||tick<0||tick>x.common.tick())return common_failure("GovernanceContinuationLifeHistoryUnavailable",id);
 auto valid=phase(x);if(!valid.ok)return valid;Integer cutoff=x.common.state().next_instance_id;if(!boundary.empty()){auto n=source_sequence(x.common,boundary);if(!n.ok)return {false,{},n.error};cutoff=n.value;}else if(tick!=x.common.tick())return common_failure("GovernanceContinuationLifeHistoryUnavailable",id);
 if(!x.death_origin)return actor->alive?common_success():common_failure("GovernanceContinuationLifeHistoryUnavailable",id);
 auto read=x.death_origin(id);if(!read.ok)return {false,{},read.error};if(!read.value)return actor->alive?common_success():common_failure("GovernanceContinuationLifeHistoryUnavailable",id);
 const auto& d=*read.value;auto n=organization_instance_sequence(d.casualty_fact_id,"military");const auto* h=x.common.state().facts.find(d.casualty_fact_id);const auto limit=x.phase?x.phase->to_tick:x.common.tick();
 if(d.actor_id!=id||d.battle_id.empty()||d.tick<0||d.tick>limit||!n||*n>=x.common.state().next_instance_id||!h||h->id!=d.casualty_fact_id||h->domain!="military"||h->kind!="military_casualties")return common_failure("GovernanceContinuationLifeHistoryUnavailable",id);
 const bool earlier=*n<cutoff;if(!boundary.empty()&&(earlier!=before(x.common,d.casualty_fact_id,boundary)||(earlier&&d.tick>tick)||(!earlier&&d.tick<tick)))return common_failure("GovernanceContinuationLifeOrderMismatch",id);
 return boundary.empty()||earlier?common_failure("GovernanceContinuationActorDead",id):common_success();
}
// 只核原已受领责任；工作和终结可在签发之后发生，不要求今天仍处于旧状态。
Outcome<GovernanceTaskSource> task_source(const PoliticalGovernanceContinuationContext& x,const StableId& task,const GovernanceAuthorityApproval& a,Integer tick,const StableId& boundary){
 auto out=x.tasks.governance_task_source(task);if(!out.ok)return out;const auto& t=out.value;const auto& s=t.acceptance;const auto fail=[&](){return Outcome<GovernanceTaskSource>{false,{},common_failure("GovernanceContinuationTaskMismatch",task).error};};
 if(s.task_id!=task||s.authority_source_fact_id!=a.authority_fact_id||s.executor_actor_id!=a.executor_actor_id||s.organization_id!=a.organization_id||s.jurisdiction_id!=a.jurisdiction_id||s.site_id!=a.site_id||s.rule_id!=a.rule_id||s.rule_version!=a.rule_version||s.work_ticks!=a.work_ticks||s.accepted_tick<a.issued_tick||tick<s.accepted_tick||tick>=s.deadline_tick||s.contract_id.empty())return fail();
 const auto sequence=[&](const StableId& id,const StableId& kind)->std::optional<Integer>{const auto* h=x.common.state().facts.find(id);const auto p=id.find(':');auto n=p==StableId::npos?std::optional<Integer>{}:organization_instance_sequence(id,id.substr(0,p));if(!n||*n>=x.common.state().next_instance_id||!h||h->id!=id||h->domain!="tasks"||h->kind!=kind)return {};return n;};
 auto accepted=sequence(s.fact_id,"governance_task_accepted");if(!accepted)return fail();
 if(boundary.empty()){if(t.completion||t.interruption||(t.task_status!="active"&&t.task_status!="ready"))return fail();}
 else{
  auto n=source_sequence(x.common,boundary);if(!n.ok)return {false,{},n.error};if(*accepted>=n.value||!before(x.common,s.fact_id,boundary))return fail();
  if(t.completion){const auto& f=*t.completion;const StableId kind=f.result==GovernanceTaskResult::completed?"governance_task_completed":f.result==GovernanceTaskResult::cancelled?"governance_task_cancelled":"governance_task_failed";auto end=sequence(f.fact_id,kind);if(!end||*end<=n.value||!before(x.common,boundary,f.fact_id)||f.completed_tick<tick)return fail();}
  if(t.interruption){auto end=sequence(t.interruption->fact_id,"governance_work_interrupted");if(!end||*end<=n.value||!before(x.common,boundary,t.interruption->fact_id)||t.interruption->stopped_tick<tick)return fail();}
 }
 return out;
}
// 同一政治裁定明确承担原金额、原钱库和原受益人；不接受另一新供给或任意公款责任。
Status execution_source(const PoliticalGovernanceContinuationContext& x,const GovernanceAuthorityApproval& a,const GovernanceTaskAcceptance& t,const PoliticalResponsibilityExecution& e){
 const auto& id=x.common.state().identity;const auto& i=e.registration.source_identity;
 if(i.world_id!=id.world_id||i.execution_epoch!=id.execution_epoch||i.revision!=id.revision||i.tick!=id.tick||e.registration_fact_id!=e.registration.registration_fact_id||e.status!=PoliticalResponsibilityExecutionStatus::approved||!e.decision)return common_failure("GovernanceContinuationTakeoverUnavailable",t.contract_id);
 const auto& d=*e.decision;const auto& r=e.registration;
 if(e.responsibility.kind!=PoliticalResponsibilityKind::relief_contract||e.responsibility.responsibility_id!=t.contract_id||d.responsibility.kind!=PoliticalResponsibilityKind::relief_contract||d.responsibility.responsibility_id!=t.contract_id||d.disposition!=PoliticalResponsibilityDisposition::assume_by_successor||!has(e.original_source_fact_ids,a.authority_fact_id)||r.jurisdiction_id!=a.jurisdiction_id||r.site_id!=a.site_id||r.successor_organization_id.empty()||!d.original_principal||d.original_principal->kind!=SubjectKind::organization||d.original_principal->id!=a.organization_id||!d.execution_principal||d.execution_principal->kind!=SubjectKind::organization||d.execution_principal->id!=r.successor_organization_id||d.payment_wallet_id!=std::optional<StableId>{a.public_wallet_id}||d.payment_inventory_id!=std::optional<StableId>{a.public_inventory_id}||!has(r.public_wallet_ids,a.public_wallet_id)||!has(r.public_inventory_ids,a.public_inventory_id))return common_failure("GovernanceContinuationTakeoverMismatch",t.contract_id);
 return common_success();
}
// 授权必须来自一份原Grant，冻结原连续性，不允许借续办扩大预算或跨库。
bool grant_covers(const Grant& g,const GovernanceAuthorityApproval& a,const ActingAuthority& signer,Integer tick){return g.office_id==signer.office_id&&g.amount_limit>=a.cash_limit&&g.valid_from<=tick&&(!g.valid_until||tick<=*g.valid_until)&&(a.executor_actor_id==signer.actor_id||g.may_delegate)&&g.continuity_policy_id==a.continuity_policy_id&&has(g.allowed_command_kinds,StableId{political_governance_continue_command})&&has(g.wallet_ids,a.public_wallet_id)&&has(g.inventory_ids,a.public_inventory_id)&&std::any_of(g.scopes.begin(),g.scopes.end(),[&](const auto& s){return s.kind==ScopeRef::Kind::site&&s.id==a.site_id;});}
Outcome<StableId> current_grant(const PoliticalGovernanceContinuationContext& x,const GovernanceAuthorityApproval& a,const ActingAuthority& signer){
 auto available=politics_detail::available(x.common,x.activity,signer.actor_id);if(!available.ok)return {false,{},available.error};auto alive=alive_before(x,signer.actor_id,x.common.tick(),{});if(!alive.ok)return {false,{},alive.error};const auto at=politics_detail::place(x.common,signer.actor_id);if(!at||at->place_id!=a.site_id)return {false,{},common_failure("GovernanceContinuationIssuerNotAtSite",signer.actor_id).error};
 OrganizationAccess org(x.common,x.organizations,x.organization_catalog,x.economy);if(!org.actor_accepts_duty(signer.actor_id,signer.organization_id))return {false,{},common_failure("GovernanceContinuationIssuerDutyDenied",signer.actor_id).error};
 const auto *office = x.organizations.offices.find(signer.office_id);
 const auto *original = x.organization_catalog.offices.find(signer.office_id);
 if (office && original)
   for (const auto &id : office->grant_ids) {
     const auto *g = x.organization_catalog.grants.find(id);
     if (!g || !has(original->grant_ids, id) ||
         !grant_covers(*g, a, signer, x.common.tick()))
       continue;
     auto proof = check_authority_with_single_grant(
         x.common, x.organizations, x.organization_catalog, signer,
         political_governance_continue_command,
         {ScopeRef::Kind::site, a.site_id}, a.cash_limit, *g, nullptr);
     if (proof.ok)
       return {true, id, {}};
   }
 return {false,{},common_failure("GovernanceContinuationGrantDenied",signer.office_id).error};
}
// 当前域控制和两容器必须仍为批准的新方；后次占领立即暂停，收回也不自动重用旧批准。
Status current_assets(const PoliticalGovernanceContinuationContext& x,const GovernanceAuthorityApproval& a,const StableId& org){const auto* j=x.politics.jurisdictions.find(a.jurisdiction_id);const auto* w=x.common.wallet(a.public_wallet_id);const auto* s=x.common.inventory(a.public_inventory_id);if(!j||j->political_organization_id!=a.organization_id||j->controlling_organization_id!=org||j->seat_place_id!=a.site_id||j->public_wallet_id!=a.public_wallet_id||!has(j->public_inventory_ids,a.public_inventory_id)||!w||!s||w->owner.kind!=Owner::Kind::organization||w->owner.id!=org||s->owner.kind!=Owner::Kind::organization||s->owner.id!=org)return common_failure("GovernanceContinuationControlChanged",a.jurisdiction_id);return common_success();}
// 永久记录只引用原来源，签发事实前必须已有实际批准和完整同ID责任。
Status record_source(const PoliticalGovernanceContinuationContext& x,const PoliticalGovernanceContinuation& r){
 auto n=source_sequence(x.common,r.id);if(!n.ok)return {false,{},n.error};if(r.tick<0||r.tick>x.common.tick())return common_failure("GovernanceContinuationTickMismatch",r.id);
 auto a=x.original_authority.governance_authority_approval(r.authority_fact_id);if(!a.ok)return {false,{},a.error};auto t=task_source(x,r.task_id,a.value,r.tick,r.id);if(!t.ok)return {false,{},t.error};const auto& accepted=t.value.acceptance;
 auto read=x.takeover.political_responsibility_before_fact({PoliticalResponsibilityKind::relief_contract,r.contract_id,{},{}},r.id,r.tick,x.common.state().identity);if(!read.ok)return {false,{},read.error};if(!read.value)return common_failure("GovernanceContinuationTakeoverUnavailable",r.id);auto actual=execution_source(x,a.value,accepted,*read.value);if(!actual.ok)return actual;const auto& e=*read.value;const auto& d=*e.decision;
 if(r.acceptance_fact_id!=accepted.fact_id||r.contract_id!=accepted.contract_id||r.executor_actor_id!=accepted.executor_actor_id||r.jurisdiction_id!=a.value.jurisdiction_id||r.site_id!=a.value.site_id||r.original_organization_id!=a.value.organization_id||r.public_wallet_id!=a.value.public_wallet_id||r.public_inventory_id!=a.value.public_inventory_id||r.registration_fact_id!=e.registration_fact_id||r.approval_fact_id!=d.takeover_fact_id||r.decision_fact_id!=d.decision_fact_id||r.occupation_fact_id!=e.registration.occupation_fact_id||r.authority.organization_id!=e.registration.successor_organization_id)return common_failure("GovernanceContinuationSourceMismatch",r.id);
 for(const auto& id:{r.registration_fact_id,r.approval_fact_id,r.decision_fact_id,r.occupation_fact_id,r.authority_fact_id}){const auto split=id.find(':');auto m=split==StableId::npos?std::optional<Integer>{}:organization_instance_sequence(id,id.substr(0,split));if(!m||*m>=n.value||!before(x.common,id,r.id))return common_failure("GovernanceContinuationSourceOrderMismatch",id);}
 const auto* office=x.organization_catalog.offices.find(r.authority.office_id);const auto* g=x.organization_catalog.grants.find(r.grant_id);if(!office||!g||!has(office->grant_ids,r.grant_id)||!grant_covers(*g,a.value,r.authority,r.tick))return common_failure("GovernanceContinuationHistoricalGrantDenied",r.id);
 actual=validate_historical_office_signer(x.common,x.organizations,x.organization_catalog,r.authority,r.tick,n.value);if(!actual.ok)return actual;actual=validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,r.authority,r.tick,r.id,PoliticalMemberHistoryUse::governance_continuation,x.phase);if(!actual.ok)return actual;
 actual=alive_before(x,r.authority.actor_id,r.tick,r.id);if(!actual.ok)return actual;return alive_before(x,r.executor_actor_id,r.tick,r.id);
}
}
namespace hanmo::resident {
// 逐事实覆盖不可删除单条载荷，也不允许同一真实决定重复制造续办来源。
Status validate_political_governance_continuations(const PoliticalGovernanceContinuationContext& x){using namespace governance_continue_detail;auto valid=phase(x);if(!valid.ok)return valid;std::set<std::pair<StableId,StableId>> used;Integer previous=0,previous_tick=-1;StableId previous_id;
 for(const auto& row:x.state.continuations.rows()){const auto& r=row.second;auto n=source_sequence(x.common,row.first);if(!n.ok)return {false,{},n.error};if(row.first!=r.id||n.value<=previous||r.tick<previous_tick||(!previous_id.empty()&&!before(x.common,previous_id,r.id))||!used.emplace(r.task_id,r.decision_fact_id).second)return common_failure("InvalidGovernanceContinuationState",row.first);valid=record_source(x,r);if(!valid.ok)return valid;previous=n.value;previous_tick=r.tick;previous_id=r.id;}
 for(const auto& row:x.common.state().facts.rows())if((row.second.domain==political_governance_continuation_module||row.second.kind==political_governance_continue_kind||row.first.rfind(StableId{political_governance_continue_prefix}+":",0)==0)&&(row.second.domain!=political_governance_continuation_module||row.second.kind!=political_governance_continue_kind||!x.state.continuations.find(row.first)))return common_failure("GovernanceContinuationPayloadMissing",row.first);
 return common_success();
}
}
