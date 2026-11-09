// 生前交接逐真实Common序复核双方与原Office；接受后紧邻的唯一更职才是原子交接结果。
#include "political_term_handover_detail.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Status validate_political_term_handover_sources(const PoliticalTermHandoverContext& x){
 using namespace term_handover_detail;if(&x.heads.common!=&x.common||&x.heads.organizations!=&x.organizations||&x.heads.organization_catalog!=&x.organization_catalog)return common_failure("PoliticalTermHandoverCandidateIdentityMismatch");auto valid=validate_political_term_handover_catalog(x.common,x.common_catalog,x.organization_catalog,x.heads.succession_catalog,x.round_catalog,x.catalog);if(!valid.ok)return valid;
 if(x.state.version!=1||x.state.offers.size()>4096||x.state.answers.size()>4096||x.state.withdrawals.size()>4096)return common_failure("InvalidPoliticalTermHandoverState");if(!x.seat||!x.player_before)return common_failure("PoliticalTermHandoverHistoricalReadersRequired");
 std::map<StableId,const PoliticalTermHandoverOffer*> seen;std::map<StableId,StableId> active;std::set<StableId> closed_ids,office_changes;std::vector<StableId> offers,answers,withdrawals;Integer prior=0,prior_tick=-1;const auto& order=x.common.state().facts.order();
 const auto player=[&](const StableId& actor,Integer tick,const StableId& fact)->Status{auto source=x.player_before(tick,fact);if(!source.ok)return {false,{},source.error};return source.value==actor?common_success():common_failure("PoliticalTermHandoverHistoricalPlayerMismatch",fact);};
 for(const auto& entry:x.common.state().facts.rows()){
  const auto& h=entry.second;const bool kind=h.kind=="political_term_handover_offered"||h.kind=="political_term_handover_accepted"||h.kind=="political_term_handover_declined"||h.kind=="political_term_handover_withdrawn";
  if(h.domain!=political_term_handover_domain){if(kind)return common_failure("PoliticalTermHandoverFactDomainMismatch",entry.first);continue;}
  const auto* o=x.state.offers.find(entry.first);const auto* a=x.state.answers.find(entry.first);const auto* w=x.state.withdrawals.find(entry.first);auto sequence=political_term_handover_fact_sequence(x.common,entry.first);
  if(!sequence||*sequence<=prior||h.id!=entry.first||Integer(bool(o))+Integer(bool(a))+Integer(bool(w))!=1)return common_failure("PoliticalTermHandoverFactCoverageMismatch",entry.first);
  const StableId expected=o?"political_term_handover_offered":w?"political_term_handover_withdrawn":a->accepted?"political_term_handover_accepted":"political_term_handover_declined";const Integer tick=o?o->issued_tick:w?w->tick:a->tick;
  if(h.kind!=expected||tick<prior_tick||tick<0||tick>x.common.tick())return common_failure("PoliticalTermHandoverFactOrderMismatch",entry.first);
  const PoliticalTermHandoverOffer* origin=o;if(!origin){auto i=seen.find(a?a->offer_fact_id:w->offer_fact_id);if(i==seen.end()||closed_ids.count(i->first))return common_failure("PoliticalTermHandoverOpenOfferRequired",entry.first);origin=i->second;}
  const auto* p=x.catalog.policies.find(origin->policy_id);const auto* original=p?original_policy(x,*p):nullptr;if(!p||!original)return common_failure("PoliticalTermHandoverOriginalPolicyRequired",entry.first);
  valid=x.seat(*original,tick,entry.first);if(!valid.ok)return valid;
  if(o){
   if(o->id!=entry.first||o->organization_id!=original->organization_id||o->office_id!=original->leader_office_id||o->issuer_actor_id==o->candidate_actor_id||o->office_epoch<0||o->head_source_fact_id.empty()||o->head_office_change_fact_id.empty()||o->issued_tick>std::numeric_limits<Integer>::max()-p->response_ticks||o->response_until!=o->issued_tick+p->response_ticks)return common_failure("PoliticalTermHandoverOfferSourceMismatch",entry.first);
   const auto previous=active.find(p->id);if(previous!=active.end()&&seen.at(previous->second)->office_epoch==o->office_epoch&&tick<seen.at(previous->second)->response_until)return common_failure("PoliticalTermHandoverDuplicateOpenOffer",entry.first);
   valid=installed_office(x,*p,*original,o->issuer_actor_id,o->office_epoch,tick,o->id,o->head_source_fact_id,o->head_office_change_fact_id);if(!valid.ok)return valid;valid=grant(x,*p,*original,o->grant_id,political_term_handover_offer_command,tick);if(!valid.ok)return valid;
   for(const auto& actor:{o->issuer_actor_id,o->candidate_actor_id}){valid=person_before(x,actor,tick,o->id);if(!valid.ok)return valid;valid=member_before(x,*original,actor,tick,o->id);if(!valid.ok)return valid;}
   valid=player(o->issuer_actor_id,tick,o->id);if(!valid.ok)return valid;seen[o->id]=o;active[p->id]=o->id;offers.push_back(o->id);
  }else if(a){
   if(a->id!=entry.first||a->actor_id!=origin->candidate_actor_id||a->expected_office_epoch!=origin->office_epoch||tick>=origin->response_until)return common_failure("PoliticalTermHandoverAnswerSourceMismatch",entry.first);
   valid=person_before(x,a->actor_id,tick,a->id);if(!valid.ok)return valid;valid=player(a->actor_id,tick,a->id);if(!valid.ok)return valid;
   if(a->accepted){
    valid=installed_office(x,*p,*original,origin->issuer_actor_id,origin->office_epoch,tick,a->id,origin->head_source_fact_id,origin->head_office_change_fact_id);if(!valid.ok)return valid;valid=grant(x,*p,*original,origin->grant_id,political_term_handover_offer_command,tick);if(!valid.ok)return valid;
    for(const auto& actor:{origin->issuer_actor_id,a->actor_id}){valid=person_before(x,actor,tick,a->id);if(!valid.ok)return valid;valid=member_before(x,*original,actor,tick,a->id);if(!valid.ok)return valid;}
    const auto* change=x.organizations.facts.find(a->office_change_fact_id);const auto* header=x.common.state().facts.find(a->office_change_fact_id);auto n=organization_instance_sequence(a->office_change_fact_id,"organization");auto pos=std::find(order.begin(),order.end(),a->id);
    if(!change||change->id!=a->office_change_fact_id||change->kind!="organization_office_changed"||change->organization_id!=origin->organization_id||change->office_id!=origin->office_id||change->actor_id!=a->actor_id||change->tick!=tick||change->amount!=0||!change->activity_id.empty()||!change->code.empty()||change->response!=MemberResponse::serving||change->cause_fact_ids!=std::vector<StableId>{a->id}||!header||header->id!=change->id||header->kind!=change->kind||header->domain!="organization"||!n||*sequence==std::numeric_limits<Integer>::max()||*n!=*sequence+1||pos==order.end()||std::next(pos)==order.end()||*std::next(pos)!=change->id||origin->office_epoch==std::numeric_limits<Integer>::max()||a->installed_office_epoch!=origin->office_epoch+1||!office_changes.insert(change->id).second)return common_failure("PoliticalTermHandoverAtomicOfficeSourceRequired",a->id);
   }else if(!a->office_change_fact_id.empty()||a->installed_office_epoch!=0)return common_failure("PoliticalTermHandoverDeclineChangedOffice",a->id);
   closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);answers.push_back(a->id);
  }else{
   if(w->id!=entry.first||w->actor_id!=origin->issuer_actor_id||w->office_epoch!=origin->office_epoch||w->grant_id!=origin->grant_id)return common_failure("PoliticalTermHandoverWithdrawalSourceMismatch",entry.first);
   valid=person_before(x,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=player(w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=installed_office(x,*p,*original,w->actor_id,w->office_epoch,tick,w->id,origin->head_source_fact_id,origin->head_office_change_fact_id);if(!valid.ok)return valid;valid=member_before(x,*original,w->actor_id,tick,w->id);if(!valid.ok)return valid;valid=grant(x,*p,*original,w->grant_id,political_term_handover_withdraw_command,tick);if(!valid.ok)return valid;
   closed_ids.insert(origin->id);if(active[p->id]==origin->id)active.erase(p->id);withdrawals.push_back(w->id);
  }
  prior=*sequence;prior_tick=tick;
 }
 if(offers!=x.state.offers.order()||answers!=x.state.answers.order()||withdrawals!=x.state.withdrawals.order())return common_failure("PoliticalTermHandoverFactCoverageMismatch");
 // 其他原组织记录也不能再借同一接受事实生成第二次换任或额外业务后果。
 for(const auto& row:x.organizations.facts.rows())for(const auto& cause:row.second.cause_fact_ids)if(x.state.answers.find(cause)&&!office_changes.count(row.first))return common_failure("PoliticalTermHandoverDuplicateOfficeChange",row.first);
 return common_success();
}
}
