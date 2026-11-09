// 原Office账仍是任职唯一账；本读只把其真实更职原因限定在已完成的两条政治业务中。
#include "political_head_source.hpp"
#include "resident/domains/politics/term_handover/political_term_handover_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using Result=Outcome<std::optional<PoliticalHeadSource>>;
Result fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
// 新轮四类本人来源之外不接受任意事实作历史权源边界。
std::optional<Integer> boundary_sequence(const CommonStore& c,const StableId& id){
 const auto* h=c.state().facts.find(id);if(!h||h->id!=id||h->domain!=political_successor_rounds_domain)return {};
 const std::pair<const char*,const char*> kinds[]={{"political_successor_round_nomination","political_successor_round_nominated"},{"political_successor_round_acceptance","political_successor_round_accepted"},{"political_successor_round_decline","political_successor_round_declined"},{"political_successor_round_withdrawal","political_successor_round_withdrawn"},{"political_successor_round_accession","political_successor_round_acceded"}};
 for(const auto& kind:kinds)if(h->kind==kind.second){auto n=organization_instance_sequence(id,kind.first);if(n&&*n<c.state().next_instance_id)return n;}return {};
}
// 真业务头必须紧接原Office更职；两源均在指定历史边界之前，原全域验证另负责双方资格。
Status exact_change(const PoliticalHeadSourceContext& x,const PoliticalSuccessionPolicy& p,const PoliticalSuccessionOfficeSource& office,const StableId& source,const StableId& prefix,const StableId& kind,const StableId& domain,Integer tick){
 const auto& c=x.common;const auto* h=c.state().facts.find(source);const auto* f=x.organizations.facts.find(office.latest_office_fact_id);const auto n=organization_instance_sequence(source,prefix),change=organization_instance_sequence(office.latest_office_fact_id,"organization");const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),source);
 if(!h||h->id!=source||h->kind!=kind||h->domain!=domain||!n||!change||*n>=c.state().next_instance_id||*n==INT64_MAX||*change!=*n+1||at==order.end()||std::next(at)==order.end()||*std::next(at)!=office.latest_office_fact_id||!f||f->id!=office.latest_office_fact_id||f->kind!="organization_office_changed"||f->organization_id!=p.organization_id||f->office_id!=p.leader_office_id||f->actor_id!=office.holder_actor_id.value_or(StableId{})||f->tick!=tick||f->cause_fact_ids!=std::vector<StableId>{source}||f->amount!=0||!f->activity_id.empty()||!f->code.empty()||f->response!=MemberResponse::serving||tick<0||tick>office.observed_tick)return common_failure("PoliticalHeadAppointmentSourceMismatch",source);
 if(!office.before_fact_id.empty()){const auto limit=std::find(order.begin(),order.end(),office.before_fact_id);if(limit==order.end()||std::next(at)>=limit)return common_failure("PoliticalHeadAppointmentAfterBoundary",source);}return common_success();
}
// 只读原Office全史后的截面，不依赖新轮本账，避免新提名历史形成递归。
Outcome<PoliticalSuccessionOfficeSource> office_at(const PoliticalHeadSourceContext& x,const PoliticalSuccessionPolicy& policy,Integer tick,const StableId& boundary,bool term=false){
 const auto* p=&policy;const auto& policy_id=p->id;auto fail=[](const StableId& code,const StableId& id)->Outcome<PoliticalSuccessionOfficeSource>{return {false,{},common_failure(code,id).error};};
 auto original=read_political_succession_office(x.common,x.organizations,x.organization_catalog,*p);if(!original.ok)return {false,{},original.error};auto office=original.value;const auto* initial=x.organization_catalog.offices.find(p->leader_office_id);
 if(tick<0||tick>x.common.tick()||(boundary.empty()&&tick!=x.common.tick()))return fail("PoliticalHeadObservationClockMismatch",policy_id);
 if(!boundary.empty()){
  const auto cutoff=term?political_term_handover_fact_sequence(x.common,boundary):boundary_sequence(x.common,boundary);const auto& order=x.common.state().facts.order();const auto limit=std::find(order.begin(),order.end(),boundary);if(!cutoff||limit==order.end())return fail("PoliticalHeadBusinessBoundaryRequired",boundary);
  office.holder_actor_id=initial->holder_actor_id;office.office_epoch=initial->epoch;office.latest_office_fact_id.clear();office.vacant=!office.holder_actor_id;office.observed_tick=tick;office.before_fact_id=boundary;
  for(const auto& row:x.organizations.facts.rows()){const auto& f=row.second;if(f.kind!="organization_office_changed"||f.office_id!=p->leader_office_id)continue;const auto n=organization_instance_sequence(row.first,"organization");const auto at=std::find(order.begin(),order.end(),row.first);if(!n||*n==*cutoff||((*n<*cutoff)!=(at<limit))||(at<limit?f.tick>tick:f.tick<tick))return fail("PoliticalHeadOfficeBoundaryMismatch",row.first);if(at<limit){++office.office_epoch;office.holder_actor_id=f.actor_id.empty()?std::nullopt:std::optional<StableId>{f.actor_id};office.latest_office_fact_id=row.first;office.vacant=!office.holder_actor_id;}}
 }
 return {true,std::move(office),{}};
}
// 先借原读核完整Office尾，再在严格新边界前复原其截面，不复制或改写原任职账。
Result read(const PoliticalHeadSourceContext& x,const StableId& policy_id,Integer tick,const StableId& boundary,bool term=false){
 const auto* p=x.succession_catalog.policies.find(policy_id);if(!p||x.succession_catalog.version!=1||x.nominations.version!=1||x.accessions.version!=1||bool(x.handover_catalog)!=bool(x.handovers)||(x.handovers&&(x.handovers->version!=1||x.handover_catalog->version!=1)))return fail("PoliticalHeadOriginalProfileRequired",policy_id);
 auto source=office_at(x,*p,tick,boundary,term);if(!source.ok)return {false,{},source.error};auto office=source.value;const auto* initial=x.organization_catalog.offices.find(p->leader_office_id);
 if(office.vacant)return {true,std::nullopt,{}};
 PoliticalHeadSource out;out.source_identity=x.common.state().identity;out.policy_id=p->id;out.organization_id=p->organization_id;out.office_id=p->leader_office_id;out.actor_id=*office.holder_actor_id;out.office_epoch=office.office_epoch;out.observed_tick=tick;out.before_fact_id=boundary;out.office_change_fact_id=office.latest_office_fact_id;
 if(office.latest_office_fact_id.empty()){if(out.actor_id!=office.original_leader_actor_id||out.office_epoch!=initial->epoch)return fail("PoliticalHeadAuthoredSourceMismatch",policy_id);return {true,std::move(out),{}};}
 const auto* change=x.organizations.facts.find(office.latest_office_fact_id);if(!change||change->cause_fact_ids.size()!=1)return fail("PoliticalHeadLegitimateAppointmentRequired",policy_id);const auto& cause=change->cause_fact_ids.front();const auto* accession=x.accessions.accessions.find(cause);const auto* answer=x.handovers?x.handovers->answers.find(cause):nullptr;if(!accession&&!answer&&x.term_handover_head){
  const auto* header=x.common.state().facts.find(cause);
  if(header&&header->kind=="political_term_handover_accepted"&&header->domain==political_term_handover_domain){
   auto later=x.term_handover_head(*p,office);if(!later.ok)return later;if(!later.value)return fail("PoliticalHeadLegitimateAppointmentRequired",cause);const auto& v=*later.value;const auto& a=v.source_identity;const auto& b=x.common.state().identity;
   if(std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)!=std::tie(b.world_id,b.execution_epoch,b.revision,b.tick)||v.policy_id!=p->id||v.organization_id!=p->organization_id||v.office_id!=p->leader_office_id||v.actor_id!=out.actor_id||v.source_fact_id!=cause||v.office_change_fact_id!=office.latest_office_fact_id||v.office_epoch!=office.office_epoch||v.observed_tick!=tick||v.before_fact_id!=boundary||v.origin!=PoliticalHeadOrigin::term_handover)return fail("PoliticalHeadTermSourceMismatch",cause);
   auto valid=exact_change(x,*p,office,cause,"political_term_handover_acceptance","political_term_handover_accepted",political_term_handover_domain,v.installed_tick);if(!valid.ok)return {false,{},valid.error};return later;
  }
 }
 if(bool(accession)==bool(answer))return fail("PoliticalHeadLegitimateAppointmentRequired",cause);
 if(accession){
  const auto* accepted=x.nominations.answers.find(accession->acceptance_fact_id);const auto* nomination=accepted?x.nominations.nominations.find(accepted->nomination_fact_id):nullptr;
  if(accession->id!=cause||!accession->player_receipt||accession->actor_id!=out.actor_id||accession->office_change_fact_id!=office.latest_office_fact_id||accession->installed_office_epoch!=out.office_epoch||!accepted||!accepted->accepted||accepted->actor_id!=out.actor_id||!nomination||nomination->policy_id!=p->id||nomination->organization_id!=p->organization_id||nomination->office_id!=p->leader_office_id||nomination->candidate_actor_id!=out.actor_id)return fail("PoliticalHeadAccessionSourceMismatch",cause);
  auto valid=exact_change(x,*p,office,cause,"political_succession_accession","political_succession_acceded",political_succession_domain,accession->tick);if(!valid.ok)return {false,{},valid.error};out.origin=PoliticalHeadOrigin::death_accession;out.installed_tick=accession->tick;out.predecessor_actor_id=nomination->issuer_actor_id;
 }else{
  const auto* offer=x.handovers->offers.find(answer->offer_fact_id);const auto* policy=offer?x.handover_catalog->policies.find(offer->policy_id):nullptr;
  if(answer->id!=cause||!answer->accepted||!answer->player_receipt||answer->actor_id!=out.actor_id||answer->office_change_fact_id!=office.latest_office_fact_id||answer->installed_office_epoch!=out.office_epoch||!offer||!policy||policy->succession_policy_id!=p->id||offer->organization_id!=p->organization_id||offer->office_id!=p->leader_office_id||offer->candidate_actor_id!=out.actor_id)return fail("PoliticalHeadHandoverSourceMismatch",cause);
  auto valid=exact_change(x,*p,office,cause,"political_head_handover_acceptance","political_head_handover_accepted",political_head_handover_domain,answer->tick);if(!valid.ok)return {false,{},valid.error};out.origin=PoliticalHeadOrigin::living_handover;out.installed_tick=answer->tick;out.predecessor_actor_id=offer->issuer_actor_id;
 }
 out.source_fact_id=cause;return {true,std::move(out),{}};
}
}
// 新轮只扩此具名入口，旧首轮与生前交接的历史白名单不受影响。
Outcome<PoliticalSuccessionOfficeSource> read_political_successor_round_office_before(const PoliticalHeadSourceContext& x,const StableId& id,Integer tick,const StableId& fact){const auto* p=x.succession_catalog.policies.find(id);if(!p||fact.empty())return {false,{},common_failure("PoliticalHeadBusinessBoundaryRequired",fact).error};return office_at(x,*p,tick,fact);}
std::optional<Integer> political_successor_round_fact_sequence(const CommonStore& c,const StableId& id){return boundary_sequence(c,id);}
// 稳定当前口不得让未来phase时点取得新首职来源。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source(const PoliticalHeadSourceContext& x,const StableId& id){return read(x,id,x.common.tick(),{});}
// 新轮payload及当前授权由其独立业务核验，此口仅证明指定事实以前已生效的原任职。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source_before(const PoliticalHeadSourceContext& x,const StableId& id,Integer tick,const StableId& fact){if(fact.empty())return fail("PoliticalHeadBusinessBoundaryRequired",id);return read(x,id,tick,fact);}
// 新后任交接边界单列，原read_political_head_source_before仍拒本段头。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source_before_term_handover(const PoliticalHeadSourceContext& x,const StableId& id,Integer tick,const StableId& fact){if(fact.empty())return fail("PoliticalHeadBusinessBoundaryRequired",id);return read(x,id,tick,fact,true);}

}
