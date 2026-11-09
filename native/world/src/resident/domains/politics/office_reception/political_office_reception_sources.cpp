// 岗位历史只从原作者和本段真实认领重建，原Common Role是唯一当前写账；不读取任意未来岗位人。
#include "political_office_reception_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
// 精确唯一头和规范正整数实例，不能仅凭domain把其它业务当作岗位移交。
std::optional<Integer> political_office_reception_fact_sequence(const CommonStore& c,const StableId& id){const auto* h=c.state().facts.find(id);if(!h||h->id!=id||h->kind!="political_office_reception_assumed"||h->domain!=political_office_reception_domain)return {};auto n=organization_instance_sequence(id,"political_office_reception");return n&&*n<c.state().next_instance_id?n:std::nullopt;}
// 原首职全链先验，再按本段实际认领的Common/分配双序取当时权源。
Outcome<PoliticalSuccessionOfficeSource> read_political_office_reception_office_before(const CommonStore& c,const OrganizationState& state,const OrganizationCatalog& catalog,const PoliticalSuccessionPolicy& policy,Integer tick,const StableId& boundary){
 const auto cutoff=political_office_reception_fact_sequence(c,boundary);const auto& order=c.state().facts.order();const auto end=std::find(order.begin(),order.end(),boundary);
 if(!cutoff||end==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("PoliticalOfficeReceptionBoundaryRequired",boundary).error};
 auto current=read_political_succession_office(c,state,catalog,policy);if(!current.ok)return current;const auto* initial=catalog.offices.find(policy.leader_office_id);auto out=current.value;out.before_fact_id=boundary;out.observed_tick=tick;out.latest_office_fact_id.clear();out.office_epoch=initial->epoch;out.holder_actor_id=initial->holder_actor_id;out.vacant=!out.holder_actor_id;
 for(const auto& row:state.facts.rows()){const auto& f=row.second;if(f.kind!="organization_office_changed"||f.office_id!=policy.leader_office_id)continue;const auto n=organization_instance_sequence(row.first,"organization");const auto at=std::find(order.begin(),order.end(),row.first);const bool before=n&&*n<*cutoff;if(!n||*n==*cutoff||before!=(at<end)||(before?f.tick>tick:f.tick<tick))return {false,{},common_failure("PoliticalOfficeReceptionOfficeOrderMismatch",row.first).error};if(before){++out.office_epoch;out.holder_actor_id=f.actor_id.empty()?std::nullopt:std::optional<StableId>{f.actor_id};out.vacant=!out.holder_actor_id;out.latest_office_fact_id=f.id;}}
 return {true,std::move(out),{}};
}
namespace {
// 本薄口证明完整岗位actor链及指定前界，完整本人/成员/死因/Grant仍由领域全验前置。
Outcome<PoliticalOfficeReceptionSource> read(const CommonStore& c,const CommonCatalog& common,const PoliticalSuccessionCatalog& originals,const PoliticalOfficeReceptionCatalog& catalog,const PoliticalOfficeReceptionState& state,const StableId& policy_id,Integer tick,const StableId& boundary){
 auto fail=[&](const StableId& code,const StableId& id=StableId{}){return Outcome<PoliticalOfficeReceptionSource>{false,{},common_failure(code,id.empty()?policy_id:id).error};};
 if(catalog.version!=1||state.version!=1||tick<0||tick>c.tick()||(boundary.empty()&&tick!=c.tick()))return fail("PoliticalOfficeReceptionClockMismatch");const auto* selected=catalog.policies.find(policy_id);if(!selected)return fail("PoliticalOfficeReceptionPolicyRequired");
 const auto& order=c.state().facts.order();const auto end=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);const auto cutoff=boundary.empty()?std::optional<Integer>{}:political_office_reception_fact_sequence(c,boundary);const auto* requested=boundary.empty()?nullptr:state.assignments.find(boundary);
 if(!boundary.empty()&&(!cutoff||end==order.end()||!requested||requested->policy_id!=policy_id||requested->tick!=tick))return fail("PoliticalOfficeReceptionBoundaryRequired",boundary);
 std::map<StableId,PoliticalOfficeReceptionSource> tails;std::set<StableId> roles;
 for(const auto& row:catalog.policies.rows()){const auto& p=row.second;const auto* original=originals.policies.find(p.succession_policy_id);const auto* role=common.roles.find(p.role_id);if(p.id!=row.first||!original||!role||!roles.insert(p.role_id).second||!c.actor(role->initial_actor)||role->organization_id!=std::optional<StableId>{original->organization_id})return fail("PoliticalOfficeReceptionOriginalRoleRequired",p.id);PoliticalOfficeReceptionSource s;s.source_identity=c.state().identity;s.policy_id=p.id;s.organization_id=original->organization_id;s.office_id=original->leader_office_id;s.role_id=p.role_id;s.actor_id=role->initial_actor;s.observed_tick=tick;s.before_fact_id=boundary;s.authored=true;tails.emplace(p.id,std::move(s));}
 auto out=tails.at(policy_id);Integer previous=0,last_tick=0;auto previous_at=order.end();std::vector<StableId> seen;
 for(const auto& row:state.assignments.rows()){const auto& a=row.second;const auto* p=catalog.policies.find(a.policy_id);const auto* original=p?originals.policies.find(p->succession_policy_id):nullptr;const auto n=political_office_reception_fact_sequence(c,row.first);const auto at=std::find(order.begin(),order.end(),row.first);
  if(row.first!=a.id||!p||!original||!n||*n<=previous||at==order.end()||(previous_at!=order.end()&&at<=previous_at)||a.tick<last_tick||a.tick>c.tick()||a.organization_id!=original->organization_id||a.office_id!=original->leader_office_id||a.role_id!=p->role_id||a.grant_id!=p->reception_grant_id||a.office_epoch<0||!c.actor(a.actor_id)||a.actor_id==a.previous_actor_id)return fail("PoliticalOfficeReceptionHistoryInvalid",row.first);
  auto& tail=tails.at(a.policy_id);if(a.previous_actor_id!=tail.actor_id||a.previous_assignment_fact_id!=tail.assignment_fact_id)return fail("PoliticalOfficeReceptionPreviousSourceMismatch",row.first);
  if(cutoff&&row.first!=boundary&&((*n<*cutoff)!=(at<end)||(at<end?a.tick>tick:a.tick<tick)))return fail("PoliticalOfficeReceptionOrderMismatch",row.first);
  tail.actor_id=a.actor_id;tail.assignment_fact_id=a.id;tail.assigned_tick=a.tick;tail.authored=false;if(a.policy_id==policy_id&&(boundary.empty()||at<end))out=tail;previous=*n;previous_at=at;last_tick=a.tick;seen.push_back(row.first);
 }
 std::vector<StableId> heads;for(const auto& row:c.state().facts.rows())if(row.second.domain==political_office_reception_domain){if(!political_office_reception_fact_sequence(c,row.first))return fail("PoliticalOfficeReceptionHeaderMismatch",row.first);heads.push_back(row.first);}if(heads!=seen)return fail("PoliticalOfficeReceptionCoverageMismatch");
 for(const auto& row:tails){const auto* role=c.state().roles.find(row.second.role_id);if(!role||role->actor_id!=row.second.actor_id)return fail("PoliticalOfficeReceptionRoleTailMismatch",row.second.role_id);}return {true,std::move(out),{}};
}
}
// 当前读证明唯一真实岗位人，不从现Office holder推定已经完成岗位接任。
Outcome<PoliticalOfficeReceptionSource> read_political_office_reception(const CommonStore& c,const CommonCatalog& common,const PoliticalSuccessionCatalog& originals,const PoliticalOfficeReceptionCatalog& catalog,const PoliticalOfficeReceptionState& state,const StableId& policy){return read(c,common,originals,catalog,state,policy,c.tick(),{});}
// 仅原实际岗位事件前的持有人，历史签名生命和服务权限由具名消费者各自验证。
Outcome<PoliticalOfficeReceptionSource> read_political_office_reception_before_assignment(const CommonStore& c,const CommonCatalog& common,const PoliticalSuccessionCatalog& originals,const PoliticalOfficeReceptionCatalog& catalog,const PoliticalOfficeReceptionState& state,const StableId& policy,Integer tick,const StableId& boundary){if(boundary.empty())return {false,{},common_failure("PoliticalOfficeReceptionBoundaryRequired").error};return read(c,common,originals,catalog,state,policy,tick,boundary);}
}
