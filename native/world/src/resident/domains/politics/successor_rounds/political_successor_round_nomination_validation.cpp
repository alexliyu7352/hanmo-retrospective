// 本意愿账按真实Common序逐项重建；后发接受、撤回或今日公职不能填入旧签署。
#include "political_successor_round_detail.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Status validate_political_successor_round_nominations(const PoliticalSuccessorRoundContext& x){
 using namespace successor_round_detail;auto valid=validate_political_successor_round_catalog(x.common,x.common_catalog,x.organization_catalog,x.heads.succession_catalog,x.catalog);if(!valid.ok)return valid;
 if(&x.common!=&x.heads.common||&x.organizations!=&x.heads.organizations||&x.organization_catalog!=&x.heads.organization_catalog)return common_failure("PoliticalSuccessorRoundCandidateMismatch");
 if(x.state.version!=1||x.state.nominations.size()>4096||x.state.answers.size()>8192||x.state.withdrawals.size()>4096||x.state.accessions.size()>4096)return common_failure("InvalidPoliticalSuccessorRoundNominationState");
 if(!x.seat||!x.player_before)return common_failure("PoliticalSuccessorRoundHistoricalReadersRequired");
 std::map<StableId,const PoliticalSuccessorRoundNomination*> seen;std::map<StableId,StableId> active,accepted_ids;std::set<StableId> closed_ids;
 std::vector<StableId> nominations,answers,withdrawals,accessions;Integer previous=0,previous_tick=-1;
 const auto historical_player=[&](const StableId& actor,Integer tick,const StableId& fact)->Status{auto who=x.player_before(tick,fact);if(!who.ok)return {false,{},who.error};return who.value==actor?common_success():common_failure("PoliticalSuccessorRoundHistoricalPlayerMismatch",fact);};
 const auto office_at=[&](const PoliticalSuccessorRoundPolicy& p,const PoliticalSuccessorRoundNomination& n,const StableId& actor,Integer epoch,Integer tick,const StableId& fact)->Status{return head(x,p,actor,epoch,n.head_source_fact_id,n.head_office_change_fact_id,tick,fact);};
 for(const auto& entry:x.common.state().facts.rows()){
  const auto& h=entry.second;if(h.domain!=political_successor_rounds_domain){if(h.kind=="political_successor_round_nominated"||h.kind=="political_successor_round_accepted"||h.kind=="political_successor_round_declined"||h.kind=="political_successor_round_withdrawn")return common_failure("PoliticalSuccessorRoundFactDomainMismatch",entry.first);continue;}
  // 真就任在此只关闭该轮意愿；完整死因/文书/Office全源随后独立核，不能借未受领记录开下一轮。
  if(h.kind=="political_successor_round_acceded"){
   const auto* result=x.state.accessions.find(entry.first);auto number=political_successor_round_fact_sequence(x.common,entry.first);const auto* acceptance=result?x.state.answers.find(result->acceptance_fact_id):nullptr;const auto found=acceptance?seen.find(acceptance->nomination_fact_id):seen.end();
   if(!result||h.id!=result->id||!number||*number<=previous||result->tick<previous_tick||result->tick>x.common.tick()||!acceptance||!acceptance->accepted||found==seen.end()||accepted_ids[found->first]!=acceptance->id||closed_ids.count(found->first)||result->actor_id!=acceptance->actor_id)return common_failure("PoliticalSuccessorRoundAccessionSourceRequired",entry.first);
   closed_ids.insert(found->first);if(active[found->second->policy_id]==found->first)active.erase(found->second->policy_id);accessions.push_back(result->id);previous=*number;previous_tick=result->tick;continue;
  }
  const auto* n=x.state.nominations.find(entry.first);const auto* a=x.state.answers.find(entry.first);const auto* w=x.state.withdrawals.find(entry.first);
  if(h.id!=entry.first||Integer(bool(n))+Integer(bool(a))+Integer(bool(w))!=1)return common_failure("PoliticalSuccessorRoundFactCoverageMismatch",entry.first);
  const StableId prefix=n?"political_successor_round_nomination":w?"political_successor_round_withdrawal":a->accepted?"political_successor_round_acceptance":"political_successor_round_decline";
  const StableId kind=n?"political_successor_round_nominated":w?"political_successor_round_withdrawn":a->accepted?"political_successor_round_accepted":"political_successor_round_declined";
  const auto number=organization_instance_sequence(entry.first,prefix);const Integer tick=n?n->issued_tick:w?w->tick:a->tick;
  if(!number||*number<=previous||*number>=x.common.state().next_instance_id||h.kind!=kind||tick<previous_tick||tick<0||tick>x.common.tick())return common_failure("PoliticalSuccessorRoundFactOrderMismatch",entry.first);
  const PoliticalSuccessorRoundNomination* origin=n;
  if(!n){auto it=seen.find(a?a->nomination_fact_id:w->nomination_fact_id);if(it==seen.end())return common_failure("PoliticalSuccessorRoundOriginalNominationRequired",entry.first);origin=it->second;if(closed_ids.count(origin->id))return common_failure("PoliticalSuccessorRoundAlreadyClosed",entry.first);}
  const auto* p=x.catalog.policies.find(origin->policy_id);const auto* original=p?x.heads.succession_catalog.policies.find(p->succession_policy_id):nullptr;if(!p||!original)return common_failure("PoliticalSuccessorRoundPolicyUnknown",entry.first);
  valid=x.seat(*original,tick,entry.first);if(!valid.ok)return valid;
  if(n){
   if(n->id!=entry.first||n->organization_id!=original->organization_id||n->office_id!=original->leader_office_id||n->head_source_fact_id.empty()||n->head_office_change_fact_id.empty()||n->issuer_actor_id==n->candidate_actor_id||n->office_epoch<0||n->issued_tick>std::numeric_limits<Integer>::max()-p->response_ticks||n->response_until!=n->issued_tick+p->response_ticks)return common_failure("PoliticalSuccessorRoundNominationSourceMismatch",entry.first);
   const auto pending=active.find(p->id);if(pending!=active.end()){const auto* old=seen.at(pending->second);if(old->office_epoch==n->office_epoch&&(accepted_ids.count(old->id)||tick<old->response_until))return common_failure("PoliticalSuccessorRoundDuplicatePendingNomination",entry.first);}
   valid=office_at(*p,*origin,n->issuer_actor_id,n->office_epoch,tick,n->id);if(!valid.ok)return valid;
   valid=grant(x,*p,*original,n->grant_id,political_successor_round_nominate_command,tick);if(!valid.ok)return valid;
   for(const auto& actor:{n->issuer_actor_id,n->candidate_actor_id}){valid=person_before(x,actor,tick,n->id);if(!valid.ok)return valid;valid=member_before(x,*original,actor,tick,n->id);if(!valid.ok)return valid;}
   valid=historical_player(n->issuer_actor_id,tick,n->id);if(!valid.ok)return valid;seen[n->id]=n;active[p->id]=n->id;nominations.push_back(n->id);
  }else if(a){
   const auto prior=accepted_ids.find(origin->id);const StableId expected=prior==accepted_ids.end()?StableId{}:prior->second;
   if(a->id!=entry.first||a->actor_id!=origin->candidate_actor_id||a->previous_acceptance_fact_id!=expected||(a->accepted&&prior!=accepted_ids.end())||(prior==accepted_ids.end()&&tick>=origin->response_until))return common_failure("PoliticalSuccessorRoundAnswerSourceMismatch",entry.first);
   valid=person_before(x,a->actor_id,tick,a->id);if(!valid.ok)return valid;valid=historical_player(a->actor_id,tick,a->id);if(!valid.ok)return valid;
   if(a->accepted){valid=person_before(x,origin->issuer_actor_id,tick,a->id);if(!valid.ok)return valid;valid=member_before(x,*original,origin->issuer_actor_id,tick,a->id);if(!valid.ok)return valid;valid=office_at(*p,*origin,origin->issuer_actor_id,origin->office_epoch,tick,a->id);if(!valid.ok)return valid;valid=grant(x,*p,*original,origin->grant_id,political_successor_round_nominate_command,tick);if(!valid.ok)return valid;valid=member_before(x,*original,a->actor_id,tick,a->id);if(!valid.ok)return valid;accepted_ids[origin->id]=a->id;}
   else{closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);}answers.push_back(a->id);
  }else{
   if(w->id!=entry.first||w->actor_id!=origin->issuer_actor_id||w->office_epoch!=origin->office_epoch||w->grant_id!=origin->grant_id)return common_failure("PoliticalSuccessorRoundWithdrawalSourceMismatch",entry.first);
   valid=person_before(x,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=historical_player(w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=office_at(*p,*origin,w->actor_id,w->office_epoch,tick,w->id);if(!valid.ok)return valid;valid=member_before(x,*original,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=grant(x,*p,*original,w->grant_id,political_successor_round_withdraw_command,tick);if(!valid.ok)return valid;
   closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);withdrawals.push_back(w->id);
  }
  previous=*number;previous_tick=tick;
 }
 if(nominations!=x.state.nominations.order()||answers!=x.state.answers.order()||withdrawals!=x.state.withdrawals.order()||accessions!=x.state.accessions.order())return common_failure("PoliticalSuccessorRoundFactCoverageMismatch");
 return common_success();
}
}
