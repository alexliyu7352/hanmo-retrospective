// 本意愿账按真实Common序逐项重建；后发接受、撤回或今日公职不能填入旧签署。
#include "political_succession_nomination_detail.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Status validate_political_succession_nominations(const PoliticalSuccessionNominationContext& x){
 using namespace succession_detail;auto valid=validate_political_succession_catalog(x.common,x.common_catalog,x.organization_catalog,x.catalog);if(!valid.ok)return valid;
 if(x.state.version!=1||x.state.nominations.size()>4096||x.state.answers.size()>8192||x.state.withdrawals.size()>4096)return common_failure("InvalidPoliticalSuccessionNominationState");
 if(!x.seat||!x.player_before)return common_failure("PoliticalSuccessionHistoricalReadersRequired");
 std::map<StableId,const PoliticalSuccessionNomination*> seen;std::map<StableId,StableId> active,accepted_ids;std::set<StableId> closed_ids;
 std::vector<StableId> nominations,answers,withdrawals;Integer previous=0,previous_tick=-1;
 const auto historical_player=[&](const StableId& actor,Integer tick,const StableId& fact)->Status{auto who=x.player_before(tick,fact);if(!who.ok)return {false,{},who.error};return who.value==actor?common_success():common_failure("PoliticalSuccessionHistoricalPlayerMismatch",fact);};
 const auto office_at=[&](const PoliticalSuccessionPolicy& p,const StableId& actor,Integer epoch,Integer tick,const StableId& fact)->Status{auto office=read_political_succession_office_before(x.common,x.organizations,x.organization_catalog,p,tick,fact);if(!office.ok)return {false,{},office.error};return office.value.holder_actor_id==std::optional<StableId>{actor}&&office.value.original_leader_actor_id==actor&&office.value.office_epoch==epoch?common_success():common_failure("PoliticalSuccessionHistoricalOfficeMismatch",fact);};
 for(const auto& entry:x.common.state().facts.rows()){
  const auto& h=entry.second;if(h.domain!=political_succession_domain){if(h.kind=="political_successor_nominated"||h.kind=="political_succession_accepted"||h.kind=="political_succession_declined"||h.kind=="political_succession_withdrawn")return common_failure("PoliticalSuccessionFactDomainMismatch",entry.first);continue;}
  // 本函数只核生前意愿；正式就任表由独立全源validator随后核验，不能遗失实际头。
  if(h.kind=="political_succession_acceded"){const auto* result=x.accessions?x.accessions->accessions.find(entry.first):nullptr;if(!result||h.id!=result->id||result->tick<0||result->tick>x.common.tick())return common_failure("PoliticalSuccessionAccessionSourceRequired",entry.first);continue;}
  const auto* n=x.state.nominations.find(entry.first);const auto* a=x.state.answers.find(entry.first);const auto* w=x.state.withdrawals.find(entry.first);
  if(h.id!=entry.first||Integer(bool(n))+Integer(bool(a))+Integer(bool(w))!=1)return common_failure("PoliticalSuccessionFactCoverageMismatch",entry.first);
  const StableId prefix=n?"political_succession_nomination":w?"political_succession_withdrawal":a->accepted?"political_succession_acceptance":"political_succession_decline";
  const StableId kind=n?"political_successor_nominated":w?"political_succession_withdrawn":a->accepted?"political_succession_accepted":"political_succession_declined";
  const auto number=organization_instance_sequence(entry.first,prefix);const Integer tick=n?n->issued_tick:w?w->tick:a->tick;
  if(!number||*number<=previous||*number>=x.common.state().next_instance_id||h.kind!=kind||tick<previous_tick||tick<0||tick>x.common.tick())return common_failure("PoliticalSuccessionFactOrderMismatch",entry.first);
  const PoliticalSuccessionNomination* origin=n;
  if(!n){auto it=seen.find(a?a->nomination_fact_id:w->nomination_fact_id);if(it==seen.end())return common_failure("PoliticalSuccessionOriginalNominationRequired",entry.first);origin=it->second;if(closed_ids.count(origin->id))return common_failure("PoliticalSuccessionAlreadyClosed",entry.first);}
  const auto* p=x.catalog.policies.find(origin->policy_id);if(!p)return common_failure("PoliticalSuccessionPolicyUnknown",entry.first);
  valid=x.seat(*p,tick,entry.first);if(!valid.ok)return valid;
  if(n){
   if(n->id!=entry.first||n->organization_id!=p->organization_id||n->office_id!=p->leader_office_id||n->issuer_actor_id==n->candidate_actor_id||n->office_epoch<0||n->issued_tick>std::numeric_limits<Integer>::max()-p->response_ticks||n->response_until!=n->issued_tick+p->response_ticks)return common_failure("PoliticalSuccessionNominationSourceMismatch",entry.first);
   const auto pending=active.find(p->id);if(pending!=active.end()){const auto* old=seen.at(pending->second);if(accepted_ids.count(old->id)||tick<old->response_until)return common_failure("PoliticalSuccessionDuplicatePendingNomination",entry.first);}
   valid=office_at(*p,n->issuer_actor_id,n->office_epoch,tick,n->id);if(!valid.ok)return valid;
   valid=grant(x,*p,n->grant_id,political_succession_nominate_command,tick);if(!valid.ok)return valid;
   for(const auto& actor:{n->issuer_actor_id,n->candidate_actor_id}){valid=person_before(x,actor,tick,n->id);if(!valid.ok)return valid;valid=member_before(x,*p,actor,tick,n->id);if(!valid.ok)return valid;}
   valid=historical_player(n->issuer_actor_id,tick,n->id);if(!valid.ok)return valid;seen[n->id]=n;active[p->id]=n->id;nominations.push_back(n->id);
  }else if(a){
   const auto prior=accepted_ids.find(origin->id);const StableId expected=prior==accepted_ids.end()?StableId{}:prior->second;
   if(a->id!=entry.first||a->actor_id!=origin->candidate_actor_id||a->previous_acceptance_fact_id!=expected||(a->accepted&&prior!=accepted_ids.end())||(prior==accepted_ids.end()&&tick>=origin->response_until))return common_failure("PoliticalSuccessionAnswerSourceMismatch",entry.first);
   valid=person_before(x,a->actor_id,tick,a->id);if(!valid.ok)return valid;valid=historical_player(a->actor_id,tick,a->id);if(!valid.ok)return valid;
   if(a->accepted){valid=person_before(x,origin->issuer_actor_id,tick,a->id);if(!valid.ok)return valid;valid=office_at(*p,origin->issuer_actor_id,origin->office_epoch,tick,a->id);if(!valid.ok)return valid;valid=member_before(x,*p,a->actor_id,tick,a->id);if(!valid.ok)return valid;accepted_ids[origin->id]=a->id;}
   else{closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);}answers.push_back(a->id);
  }else{
   if(w->id!=entry.first||w->actor_id!=origin->issuer_actor_id||w->office_epoch!=origin->office_epoch||w->grant_id!=origin->grant_id)return common_failure("PoliticalSuccessionWithdrawalSourceMismatch",entry.first);
   valid=person_before(x,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=historical_player(w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=office_at(*p,w->actor_id,w->office_epoch,tick,w->id);if(!valid.ok)return valid;valid=member_before(x,*p,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=grant(x,*p,w->grant_id,political_succession_withdraw_command,tick);if(!valid.ok)return valid;
   closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);withdrawals.push_back(w->id);
  }
  previous=*number;previous_tick=tick;
 }
 if(nominations!=x.state.nominations.order()||answers!=x.state.answers.order()||withdrawals!=x.state.withdrawals.order())return common_failure("PoliticalSuccessionFactCoverageMismatch");
 return common_success();
}
}
