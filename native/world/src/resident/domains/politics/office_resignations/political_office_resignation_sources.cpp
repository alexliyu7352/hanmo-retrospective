// 同tick的任职变化必须在全局分配序与Common事实序同侧；今日官职不能倒证旧申请或答复。
#include "political_office_resignation_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
std::optional<Integer> political_office_resignation_fact_sequence(const CommonStore& c,const StableId& id,const StableId& kind){
 StableId prefix;if(kind=="political_office_resignation_requested")prefix="political_office_resignation_request";else if(kind=="political_office_resignation_answered")prefix="political_office_resignation_answer";else if(kind=="political_office_resignation_withdrawn")prefix="political_office_resignation_withdrawal";else return {};
 const auto number=organization_instance_sequence(id,prefix);const auto* h=c.state().facts.find(id);
 if(!number||*number>=c.state().next_instance_id||!h||h->id!=id||h->kind!=kind||h->domain!=political_office_resignation_module)return {};return number;
}
namespace {
Outcome<PoliticalOfficeResignationOfficeSource> read(const CommonStore& c,const OrganizationState& state,const OrganizationCatalog& catalog,const StableId& office,Integer tick,const StableId& boundary){
 auto fail=[&](const StableId& code)->Outcome<PoliticalOfficeResignationOfficeSource>{return {false,{},common_failure(code,office).error};};
 const auto* initial=catalog.offices.find(office);const auto* current=state.offices.find(office);const auto& order=c.state().facts.order();auto end=order.end();std::optional<Integer> cutoff;
 if(tick<0||tick>c.tick()||!initial||!current||initial->id!=office||current->id!=office||initial->organization_id!=current->organization_id||initial->epoch<0||initial->epoch>current->epoch)return fail("PoliticalOfficeResignationOfficeSourceRequired");
 if(!boundary.empty()){const auto* h=c.state().facts.find(boundary);cutoff=h?political_office_resignation_fact_sequence(c,boundary,h->kind):std::nullopt;end=std::find(order.begin(),order.end(),boundary);if(!cutoff||end==order.end())return fail("PoliticalOfficeResignationBoundaryRequired");}else if(tick!=c.tick())return fail("PoliticalOfficeResignationStableReadRequired");
 auto holder=initial->holder_actor_id,observed_holder=holder;Integer epoch=initial->epoch,observed_epoch=epoch,previous=0,last_tick=0;std::size_t previous_position=0;bool have_previous=false;StableId last_change;std::set<Integer> numbers;
 for(const auto& row:state.facts.rows()){
  const auto& f=row.second;if(f.kind!="organization_office_changed"||f.office_id!=office)continue;
  const auto n=organization_instance_sequence(f.id,"organization");const auto* h=c.state().facts.find(f.id);auto at=std::find(order.begin(),order.end(),f.id);
  if(row.first!=f.id||!n||*n<=previous||*n>=c.state().next_instance_id||!numbers.insert(*n).second||!h||h->id!=f.id||h->kind!=f.kind||h->domain!="organization"||at==order.end()||f.organization_id!=initial->organization_id||f.tick<last_tick||f.tick>c.tick()||epoch==std::numeric_limits<Integer>::max()||(!f.actor_id.empty()&&!c.actor(f.actor_id)))return fail("PoliticalOfficeResignationOfficeHistoryMismatch");
  const auto position=static_cast<std::size_t>(at-order.begin());if(have_previous&&position<=previous_position)return fail("PoliticalOfficeResignationOfficeOrderMismatch");
  const bool earlier=!cutoff||*n<*cutoff;if(cutoff&&(*n==*cutoff||earlier!=(at<end)||(earlier&&f.tick>tick)||(!earlier&&f.tick<tick)))return fail("PoliticalOfficeResignationOfficeBoundaryMismatch");
  ++epoch;holder=f.actor_id.empty()?std::nullopt:std::optional<StableId>{f.actor_id};if(earlier){observed_epoch=epoch;observed_holder=holder;last_change=f.id;}previous=*n;last_tick=f.tick;previous_position=position;have_previous=true;
 }
 if(epoch!=current->epoch||holder!=current->holder_actor_id||(holder.has_value()!=(current->status==OfficeStatus::active)))return fail("PoliticalOfficeResignationOfficeCurrentMismatch");
 return {true,{c.state().identity,initial->organization_id,office,last_change,boundary,observed_holder,observed_epoch,tick},{}};
}
}
Outcome<PoliticalOfficeResignationOfficeSource> read_political_office_resignation_office(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& cat,const StableId& office){return read(c,s,cat,office,c.tick(),{});}
Outcome<PoliticalOfficeResignationOfficeSource> read_political_office_resignation_office_before(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& cat,const StableId& office,Integer tick,const StableId& boundary){if(boundary.empty())return {false,{},common_failure("PoliticalOfficeResignationBoundaryRequired",office).error};return read(c,s,cat,office,tick,boundary);}
}
