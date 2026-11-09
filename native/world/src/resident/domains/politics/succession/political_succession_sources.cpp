// 原首职历史按Common实际顺序与原实例分配双重核验，今日持有人不能倒填提名时权源。
#include "political_succession_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <limits>
namespace hanmo::resident {
// 新业务只读自己明确的五类边界，不接受任意真实事实或未来tick。
std::optional<Integer> political_succession_fact_sequence(const CommonStore& common,const StableId& id){
 const auto* h=common.state().facts.find(id);if(!h||h->id!=id||h->domain!=political_succession_domain)return {};
 const std::pair<const char*,const char*> allowed[]={{"political_succession_nomination","political_successor_nominated"},{"political_succession_acceptance","political_succession_accepted"},{"political_succession_decline","political_succession_declined"},{"political_succession_withdrawal","political_succession_withdrawn"},{"political_succession_accession","political_succession_acceded"}};
 for(const auto& item:allowed)if(h->kind==item.second){auto n=organization_instance_sequence(id,item.first);if(n&&*n<common.state().next_instance_id)return n;}return {};
}
namespace {
// 只有显式原组织的原首职可被本域读取；不会靠同名政策跨组织择取空职。
Outcome<PoliticalSuccessionOfficeSource> read(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const PoliticalSuccessionPolicy& policy,Integer tick,const StableId& boundary){
 auto fail=[&](const StableId& code){return Outcome<PoliticalSuccessionOfficeSource>{false,{},common_failure(code,policy.id).error};};
 const auto* initial_org=catalog.organizations.find(policy.organization_id);const auto* org=state.organizations.find(policy.organization_id);
 const auto* initial=catalog.offices.find(policy.leader_office_id);const auto* current=state.offices.find(policy.leader_office_id);
 if(policy.id.empty()||!initial_org||!org||!initial||!current||initial_org->succession_policy_id!=policy.id||org->succession_policy_id!=policy.id||initial_org->headquarters_site!=policy.seat_place_id||org->headquarters_site!=policy.seat_place_id||initial->organization_id!=policy.organization_id||current->organization_id!=policy.organization_id||!initial->holder_actor_id||initial->status!=OfficeStatus::active||initial->epoch<0||!common.actor(*initial->holder_actor_id)||std::find(initial_org->office_ids.begin(),initial_org->office_ids.end(),initial->id)==initial_org->office_ids.end())return fail("PoliticalSuccessionOriginalOfficeRequired");
 if(tick<0||tick>common.tick()||(boundary.empty()&&tick!=common.tick()))return fail("PoliticalSuccessionOfficeClockMismatch");
 const auto& order=common.state().facts.order();const auto limit=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);auto cutoff=boundary.empty()?std::optional<Integer>{}:political_succession_fact_sequence(common,boundary);
 if(!boundary.empty()&&(!cutoff||limit==order.end()))return fail("PoliticalSuccessionBoundaryRequired");
 auto holder=initial->holder_actor_id;Integer epoch=initial->epoch,prior_number=0,prior_tick=0;auto prior_position=order.end();std::vector<StableId> selected;
 PoliticalSuccessionOfficeSource out{common.state().identity,policy.id,policy.organization_id,initial->id,policy.seat_place_id,*initial->holder_actor_id,{},boundary,holder,epoch,tick,false};
 for(const auto& row:state.facts.rows()){
  const auto& f=row.second;if(f.kind!="organization_office_changed"||f.office_id!=initial->id)continue;
  const auto n=organization_instance_sequence(row.first,"organization");const auto* head=common.state().facts.find(row.first);const auto at=std::find(order.begin(),order.end(),row.first);
  if(row.first!=f.id||!n||*n<=prior_number||*n>=common.state().next_instance_id||!head||head->id!=row.first||head->kind!=f.kind||head->domain!="organization"||at==order.end()||(prior_position!=order.end()&&at<=prior_position)||f.organization_id!=policy.organization_id||f.tick<prior_tick||f.tick>common.tick()||(!f.actor_id.empty()&&!common.actor(f.actor_id))||epoch==std::numeric_limits<Integer>::max())return fail("PoliticalSuccessionOfficeHistoryInvalid");
  if(cutoff&&((*n==*cutoff)||((*n<*cutoff)!=(at<limit))||(at<limit?f.tick>tick:f.tick<tick)))return fail("PoliticalSuccessionOfficeOrderMismatch");
  ++epoch;holder=f.actor_id.empty()?std::nullopt:std::optional<StableId>{f.actor_id};
  if(boundary.empty()||at<limit){out.office_epoch=epoch;out.holder_actor_id=holder;out.latest_office_fact_id=f.id;out.vacant=!holder;}
  prior_number=*n;prior_tick=f.tick;prior_position=at;selected.push_back(row.first);
 }
 // 目标Office的每个持久载荷必须对应实际Common头；其余组织史由原完整validator负责。
 std::vector<StableId> common_selected;for(const auto& id:order){const auto* f=state.facts.find(id);if(f&&f->kind=="organization_office_changed"&&f->office_id==initial->id)common_selected.push_back(id);}
 if(selected!=common_selected||epoch!=current->epoch||holder!=current->holder_actor_id||current->status!=(holder?OfficeStatus::active:OfficeStatus::vacant))return fail("PoliticalSuccessionOfficeTailMismatch");
 return {true,std::move(out),{}};
}
}
// 当前报价只读当前稳定钟，不接受尚未提交的phase.to。
Outcome<PoliticalSuccessionOfficeSource> read_political_succession_office(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const PoliticalSuccessionPolicy& p){return read(c,s,catalog,p,c.tick(),{});}
// 调用域仍须逐条验证指定业务事实的tick、签名及载荷，不能拿本读口替代正式继位全验。
Outcome<PoliticalSuccessionOfficeSource> read_political_succession_office_before(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const PoliticalSuccessionPolicy& p,Integer tick,const StableId& fact){if(fact.empty())return {false,{},common_failure("PoliticalSuccessionBoundaryRequired").error};return read(c,s,catalog,p,tick,fact);}
}
