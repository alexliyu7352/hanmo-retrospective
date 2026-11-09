// 后来任期交接历史复用原首职完整链，再仅按本业务实际头截断；不放宽死亡继位1的边界名单。
#include "political_term_handover_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
// 精确四种本段头与规范实例，不把任意政治头当历史边界。
std::optional<Integer> political_term_handover_fact_sequence(const CommonStore& common,const StableId& id){
 const auto* h=common.state().facts.find(id);if(!h||h->id!=id||h->domain!=political_term_handover_domain)return {};
 const std::pair<const char*,const char*> allowed[]={{"political_term_handover_offer","political_term_handover_offered"},{"political_term_handover_acceptance","political_term_handover_accepted"},{"political_term_handover_decline","political_term_handover_declined"},{"political_term_handover_withdrawal","political_term_handover_withdrawn"}};
 for(const auto& item:allowed)if(h->kind==item.second){auto n=organization_instance_sequence(id,item.first);if(n&&*n<common.state().next_instance_id)return n;}return {};
}
// 原完整Office尾先验后按同tick双序取此前任职，当前Owner或副手不代来源。
Outcome<PoliticalSuccessionOfficeSource> read_political_term_handover_office_before(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const PoliticalSuccessionPolicy& policy,Integer tick,const StableId& boundary){
 const auto cutoff=political_term_handover_fact_sequence(common,boundary);const auto& order=common.state().facts.order();const auto end=std::find(order.begin(),order.end(),boundary);
 if(!cutoff||end==order.end()||tick<0||tick>common.tick())return {false,{},common_failure("PoliticalTermHandoverBoundaryRequired",boundary).error};
 // 先验原链每条头/载荷/顺序及末任职一致；此薄口不替代原Organization完整史或业务签名。
 auto current=read_political_succession_office(common,state,catalog,policy);if(!current.ok)return current;const auto* initial=catalog.offices.find(policy.leader_office_id);
 auto out=current.value;out.before_fact_id=boundary;out.observed_tick=tick;out.latest_office_fact_id.clear();out.office_epoch=initial->epoch;out.holder_actor_id=initial->holder_actor_id;out.vacant=!out.holder_actor_id;
 for(const auto& row:state.facts.rows()){
  const auto& f=row.second;if(f.kind!="organization_office_changed"||f.office_id!=policy.leader_office_id)continue;
  const auto sequence=organization_instance_sequence(row.first,"organization");const auto at=std::find(order.begin(),order.end(),row.first);const bool before=sequence&&*sequence<*cutoff;
  if(!sequence||*sequence==*cutoff||before!=(at<end)||(before?f.tick>tick:f.tick<tick))return {false,{},common_failure("PoliticalTermHandoverOfficeOrderMismatch",row.first).error};
  if(before){++out.office_epoch;out.holder_actor_id=f.actor_id.empty()?std::nullopt:std::optional<StableId>{f.actor_id};out.vacant=!out.holder_actor_id;out.latest_office_fact_id=f.id;}
 }
 return {true,std::move(out),{}};
}
}
