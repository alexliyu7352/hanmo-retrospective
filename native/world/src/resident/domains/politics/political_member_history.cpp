#include "political_member_history.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include <algorithm>
#include <map>
#include <limits>
namespace hanmo::resident {
Status validate_political_historical_member_duty(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const ActingAuthority& authority,Integer tick,const StableId& boundary,PoliticalMemberHistoryUse use,const DynamicStepInput* phase,const OrganizationAuthorityClock* clock,const WorldMoment* actual_boundary){
 const StableId unavailable=use==PoliticalMemberHistoryUse::political_dismissal?"PoliticalDismissalMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_subordinate_appointment?"PoliticalSubordinateMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_governance_agency?"PoliticalGovernanceAgencyMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_office_reception?"PoliticalOfficeReceptionMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_term_handover?"PoliticalTermHandoverMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_successor_round?"PoliticalSuccessorRoundMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_head_handover?"PoliticalHeadHandoverMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::political_succession?"PoliticalSuccessionMemberHistoryUnavailable":use==PoliticalMemberHistoryUse::shop_licensing?"PoliticalShopMemberHistoryUnavailable":"GovernanceContinuationMemberHistoryUnavailable";
 const StableId denied_code=use==PoliticalMemberHistoryUse::political_dismissal?"PoliticalDismissalHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_subordinate_appointment?"PoliticalSubordinateHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_governance_agency?"PoliticalGovernanceAgencyHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_office_reception?"PoliticalOfficeReceptionHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_term_handover?"PoliticalTermHandoverHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_successor_round?"PoliticalSuccessorRoundHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_head_handover?"PoliticalHeadHandoverHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::political_succession?"PoliticalSuccessionHistoricalMemberDutyDenied":use==PoliticalMemberHistoryUse::shop_licensing?"PoliticalShopHistoricalMemberDutyDenied":"GovernanceContinuationHistoricalMemberDutyDenied";
 const auto fail=[&](const StableId& id=StableId{}){return common_failure(unavailable,id.empty()?authority.actor_id:id);};
 auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&(!clock||!actual_boundary))return common_failure("PoliticalMemberExactBoundaryRequired",boundary);if(actual_boundary){auto compared=compare_world_moments(*actual_boundary,now.value);if(!compared.ok)return {false,{},compared.error};if(compared.value>0||actual_boundary->half_day_tick!=tick)return fail(boundary);}
 if(!catalog.enabled||authority.actor_id.empty()||authority.organization_id.empty()||!common.actor(authority.actor_id)||tick<0||tick>now.value.half_day_tick)return fail();
 const auto* head=common.state().facts.find(boundary);if(!head||head->id!=boundary)return fail(boundary);StableId prefix;
 if(use==PoliticalMemberHistoryUse::shop_licensing){
  if(head->kind=="political_shop_registry_accepted"&&head->domain=="political_shop_registry_successions")prefix="political_shop_registry_acceptance";
  else if(head->domain=="political_shop_licensing"){
   if(head->kind=="political_wanted_registered")prefix="political_wanted";
   else if(head->kind=="political_wanted_released")prefix="political_wanted_release";
   else if(head->kind=="political_shop_clearance_recorded")prefix="political_shop_clearance";
   else if(head->kind=="political_shop_permit_revoked")prefix="political_shop_revocation";
  }
 }else if(use==PoliticalMemberHistoryUse::political_dismissal&&head->kind=="political_office_dismissed"&&head->domain=="politics")prefix="politics";
 else if(use==PoliticalMemberHistoryUse::political_office_reception&&head->kind=="political_office_reception_assumed"&&head->domain=="political_office_receptions")prefix="political_office_reception";
 else if(use==PoliticalMemberHistoryUse::political_subordinate_appointment&&head->domain=="political_subordinate_appointments"){
  if(head->kind=="political_subordinate_appointment_offered")prefix="political_subordinate_offer";
  else if(head->kind=="political_subordinate_appointment_accepted")prefix="political_subordinate_answer";
  else if(head->kind=="political_subordinate_appointment_withdrawn")prefix="political_subordinate_withdrawal";
  else if(head->kind=="political_subordinate_appointment_arrived")prefix="political_subordinate_arrival";
 }
 else if(use==PoliticalMemberHistoryUse::political_governance_agency&&head->domain=="political_governance_agencies"){
  if(head->kind=="political_governance_agency_offered")prefix="political_governance_agency_offer";
  else if(head->kind=="political_governance_agency_accepted"||head->kind=="political_governance_agency_declined")prefix="political_governance_agency_answer";
  else if(head->kind=="political_governance_agency_revoked")prefix="political_governance_agency_revocation";
  else if(head->kind=="political_governance_agency_issued")prefix="political_governance_agency_issue";
 } else if(use==PoliticalMemberHistoryUse::governance_continuation&&head->kind=="political_governance_continued"&&head->domain=="political_governance_continuations")prefix="political_governance_continuation";
 else if(use==PoliticalMemberHistoryUse::political_succession&&head->domain=="political_successions"){
  if(head->kind=="political_successor_nominated")prefix="political_succession_nomination";
  else if(head->kind=="political_succession_accepted")prefix="political_succession_acceptance";
  else if(head->kind=="political_succession_withdrawn")prefix="political_succession_withdrawal";
  else if(head->kind=="political_succession_acceded")prefix="political_succession_accession";
 }else if(use==PoliticalMemberHistoryUse::political_successor_round&&head->domain=="political_successor_rounds"){
  if(head->kind=="political_successor_round_nominated")prefix="political_successor_round_nomination";
  else if(head->kind=="political_successor_round_accepted")prefix="political_successor_round_acceptance";
  else if(head->kind=="political_successor_round_withdrawn")prefix="political_successor_round_withdrawal";
  else if(head->kind=="political_successor_round_acceded")prefix="political_successor_round_accession";
 }else if(use==PoliticalMemberHistoryUse::political_term_handover&&head->domain=="political_term_handovers"){
  if(head->kind=="political_term_handover_offered")prefix="political_term_handover_offer";
  else if(head->kind=="political_term_handover_accepted")prefix="political_term_handover_acceptance";
  else if(head->kind=="political_term_handover_withdrawn")prefix="political_term_handover_withdrawal";
 }else if(use==PoliticalMemberHistoryUse::political_head_handover&&head->domain=="political_head_handovers"){
  if(head->kind=="political_head_handover_offered")prefix="political_head_handover_offer";
  else if(head->kind=="political_head_handover_accepted")prefix="political_head_handover_acceptance";
  else if(head->kind=="political_head_handover_withdrawn")prefix="political_head_handover_withdrawal";
 }
 auto cutoff=prefix.empty()?std::optional<Integer>{}:organization_instance_sequence(boundary,prefix);
 if(!cutoff||*cutoff>=common.state().next_instance_id)return fail(boundary);
 Integer limit=now.value.half_day_tick;if(phase){if(limit==std::numeric_limits<Integer>::max()||phase->from_tick!=limit||phase->to_tick!=limit+1)return fail();limit=phase->to_tick;}
 std::map<StableId,std::size_t> positions;for(std::size_t i=0;i<common.state().facts.size();++i)if(!positions.emplace(common.state().facts.rows()[i].first,i).second)return fail();
 const auto before=[&](const StableId& a,const StableId& b){return positions.count(a)&&positions.count(b)&&positions.at(a)<positions.at(b);};
 const auto fact=[&](const StableId& id,const StableId& kind)->std::optional<Integer>{const auto* h=common.state().facts.find(id);const auto* f=state.facts.find(id);auto n=organization_instance_sequence(id,"organization");if(!h||h->id!=id||h->kind!=kind||h->domain!="organization"||!f||f->id!=id||f->kind!=kind||!n||*n>=common.state().next_instance_id)return {};return n;};
 struct Episode {const OrgMemberState* saved=nullptr;MemberResponse response=MemberResponse::serving;Integer since=0;StableId last;bool visited=false;};
 std::map<StableId,Episode> episodes;std::map<StableId,StableId> joins;StableId active;bool permitted=false;Integer seed_count=0;
 for(const auto& row:state.members.rows()){
  const auto& m=row.second;if(m.actor_id!=authority.actor_id||m.organization_id!=authority.organization_id)continue;
  if(row.first!=m.id||m.id.empty()||!catalog.member_policies.find(m.policy_id))return fail(row.first);
  const auto* initial=catalog.members.find(m.id);
  if(initial){
   if(m.episode_source||initial->episode_source||initial->actor_id!=m.actor_id||initial->organization_id!=m.organization_id||initial->policy_id!=m.policy_id||initial->response_since_tick!=0||!initial->last_fact_id.empty()||++seed_count>1)return fail(m.id);
   auto value=static_cast<Integer>(initial->response);if(value<0||value>static_cast<Integer>(MemberResponse::departed))return fail(m.id);
   episodes.emplace(m.id,Episode{&m,initial->response,0,{},true});if(initial->response!=MemberResponse::departed)active=m.id;
   permitted=initial->response==MemberResponse::serving||initial->response==MemberResponse::payment_requested;
  }else{
   if(!catalog.named_membership_enabled||!m.episode_source)return fail(m.id);const auto& source=*m.episode_source;
   const auto* offer=state.membership_offers.find(source.offer_id);const auto* decision=state.membership_decisions.find(source.decision_id);const auto* terms=catalog.membership_terms.find(source.terms_id);const auto* joined=state.facts.find(source.joined_fact_id);
   auto decision_sequence=fact(source.decision_id,"membership_decided"),joined_sequence=fact(source.joined_fact_id,"organization_member_response");
   if(!offer||!decision||!terms||!joined||!decision_sequence||!joined_sequence||*decision_sequence>=*joined_sequence||!before(source.decision_id,source.joined_fact_id)||offer->status!=MembershipOfferStatus::accepted||offer->member_episode_id!=m.id||offer->target_actor_id!=m.actor_id||offer->organization_id!=m.organization_id||offer->decision_fact_id!=source.decision_id||offer->terms_id!=source.terms_id||decision->id!=source.decision_id||decision->offer_id!=offer->id||decision->target_actor_id!=m.actor_id||decision->kind!=MembershipDecisionKind::accept||decision->tick!=source.joined_tick||terms->organization_id!=m.organization_id||terms->member_policy_id!=m.policy_id||joined->actor_id!=m.actor_id||joined->organization_id!=m.organization_id||joined->response!=MemberResponse::serving||joined->tick!=source.joined_tick||std::find(joined->cause_fact_ids.begin(),joined->cause_fact_ids.end(),decision->id)==joined->cause_fact_ids.end()||!joins.emplace(source.joined_fact_id,m.id).second)return fail(m.id);
   episodes.emplace(m.id,Episode{&m,MemberResponse::serving,source.joined_tick,{},false});
  }
 }
 // 初始成员不能从状态删去后靠更晚加入回填；同院签字只读原组织本人序列。
 for(const auto& row:catalog.members.rows())if(row.second.actor_id==authority.actor_id&&row.second.organization_id==authority.organization_id&&!episodes.count(row.first))return fail(row.first);
 std::map<Integer,const OrganizationFact*> responses;for(const auto& row:state.facts.rows()){
  const auto& f=row.second;if(f.kind!="organization_member_response"||f.actor_id!=authority.actor_id||f.organization_id!=authority.organization_id)continue;auto n=fact(row.first,f.kind);if(row.first!=f.id||!n||f.tick<0||f.tick>limit||!responses.emplace(*n,&f).second)return fail(row.first);
 }
 StableId previous;Integer last_tick=-1;for(const auto& entry:responses){const auto& f=*entry.second;
  if(f.tick<last_tick||(!previous.empty()&&!before(previous,f.id))||entry.first==*cutoff)return fail(f.id);
  const bool earlier=entry.first<*cutoff;if(earlier!=before(f.id,boundary)||(earlier&&f.tick>tick)||(!earlier&&f.tick<tick))return fail(f.id);
  if(catalog.exact_fact_clock_enabled){if(!f.occurred_at||f.tick!=f.occurred_at->half_day_tick)return fail(f.id);auto live=compare_world_moments(*f.occurred_at,now.value);if(!live.ok)return {false,{},live.error};auto relation=compare_world_moments(*f.occurred_at,*actual_boundary);if(!relation.ok)return {false,{},relation.error};if(live.value>0||(earlier&&relation.value>0)||(!earlier&&relation.value<0))return fail(f.id);}
  const auto joined=joins.find(f.id);if(joined!=joins.end()){if(!active.empty()||episodes.at(joined->second).visited)return fail(f.id);active=joined->second;episodes.at(active).visited=true;}
  if(active.empty())return fail(f.id);auto& e=episodes.at(active);const auto response=static_cast<Integer>(f.response);if(response<0||response>static_cast<Integer>(MemberResponse::departed))return fail(f.id);
  e.response=f.response;e.since=f.tick;e.last=f.id;if(f.response==MemberResponse::departed)active.clear();
  if(earlier)permitted=!active.empty()&&(f.response==MemberResponse::serving||f.response==MemberResponse::payment_requested);
  previous=f.id;last_tick=f.tick;
 }
 for(const auto& row:episodes){const auto& e=row.second;const auto& saved=*e.saved;if(!e.visited||saved.response!=e.response||saved.response_since_tick!=e.since||saved.last_fact_id!=e.last)return fail(saved.id);
  if(saved.episode_source){const auto& source=*saved.episode_source;if(e.response==MemberResponse::departed){if(source.departure_fact_id!=e.last||source.departed_tick!=std::optional<Integer>{e.since})return fail(saved.id);}else if(source.departed_tick||!source.departure_fact_id.empty())return fail(saved.id);}
 }
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="organization"&&row.second.kind=="organization_member_response"&&!state.facts.find(row.first))return fail(row.first);
 return permitted?common_success():common_failure(denied_code,authority.actor_id);
}
}
