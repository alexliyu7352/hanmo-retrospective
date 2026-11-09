// 原交易/任职/出勤仍留在原账；本投影只用于证明真实提交前后新增的是哪一个原事实。
#include "private_shop_command_result_sources.hpp"
#include "resident/domains/commerce/private_shop_codec.hpp"
#include "resident/domains/commerce/private_shop_payload.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
BoundaryValue stable(const PrivateShopRecord& value){auto v=value;v.status.clear();v.markup_basis_points=0;return dynamic_economy_codec::encode(v);}
BoundaryValue stable(const PrivateShopOperation& value){auto v=value;v.result_fact_id.clear();v.attendance_ticks.clear();return dynamic_economy_codec::encode(v);}
BoundaryValue stable(const PrivateShopStaffContract& value){auto v=value;v.status.clear();v.terminal_fact_id.clear();return dynamic_economy_codec::encode(v);}
BoundaryValue stable(const PrivateShopOrderRecord& value){auto v=value;v.status.clear();v.task_id.clear();v.acceptance_fact_id.clear();v.binding_fact_id.clear();v.terms.accepted_tick=-1;return dynamic_economy_codec::encode(v);}
template<class T>BoundaryValue stable(const T& value){return dynamic_economy_codec::encode(value);}
std::optional<Integer> number(const CommonStore& c,const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return {};auto n=organization_instance_sequence(id,id.substr(0,at));return n&&*n<c.state().next_instance_id?n:std::nullopt;}
}
Outcome<std::vector<PrivateShopCommandFactSource>> read_private_shop_command_fact_sources(const CommonStore& c,const PrivateShopState& s,ProjectionWriter* projection){
 if(s.version!=1)return fail<std::vector<PrivateShopCommandFactSource>>("UnsupportedPrivateShopVersion");std::map<StableId,PrivateShopCommandFactSource> records;Status valid=common_success();
 auto add=[&](const auto& table,auto tick){for(const auto& row:table.rows()){if(!valid.ok)return;const auto* h=c.state().facts.find(row.first);if(!h){valid=common_failure("PrivateShopResultSourceMissing",row.first);return;}valid=projection?validate_private_shop_fact_header(c,s,*h,*projection):validate_private_shop_fact_header(c,s,*h);if(!valid.ok)return;const auto t=tick(row.second);if(!number(c,row.first)||t<0||t>c.tick()||!records.emplace(row.first,PrivateShopCommandFactSource{row.first,h->kind,t,stable(row.second)}).second){valid=common_failure("PrivateShopResultSourceMismatch",row.first);return;}}};
 add(s.shops,[](const auto& v){return v.opened_tick;});add(s.capital,[](const auto& v){return v.tick;});add(s.markups,[](const auto& v){return v.tick;});add(s.purchases,[](const auto& v){return v.tick;});add(s.operations,[](const auto& v){return v.start_tick;});add(s.operating_results,[](const auto& v){return v.tick;});add(s.staff.contracts,[](const auto& v){return v.accepted_tick;});add(s.staff.decisions,[](const auto& v){return v.tick;});add(s.staff.services,[](const auto& v){return v.tick;});add(s.orders.orders,[](const auto& v){return v.issued_tick;});add(s.orders.bindings,[](const auto& v){return v.tick;});add(s.order_settlements,[](const auto& v){return v.tick;});add(s.closings,[](const auto& v){return v.tick;});if(!valid.ok)return {false,{},valid.error};
 std::vector<PrivateShopCommandFactSource> out;Integer previous=0;std::set<StableId> seen;for(const auto& h:c.state().facts.rows())if(h.second.domain==private_shop_module){const auto found=records.find(h.first);const auto n=number(c,h.first);if(found==records.end()||!n||*n<=previous||!seen.insert(h.first).second)return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultSourceCoverageMismatch",h.first);previous=*n;out.push_back(found->second);}if(out.size()!=records.size())return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultSourceCoverageMismatch");return {true,std::move(out),{}};
}
Outcome<std::vector<PrivateShopCommandFactSource>> private_shop_new_command_fact_sources(const CommonStore& before,const PrivateShopState& old_shops,const CommonStore& c,const PrivateShopState& shops){
 const auto& a=before.state().identity;const auto& b=c.state().identity;if(a.world_id!=b.world_id||a.execution_epoch!=b.execution_epoch||b.tick<a.tick||b.revision<a.revision||a.revision==INT64_MAX||b.revision>a.revision+1||before.state().next_instance_id<1||c.state().next_instance_id<before.state().next_instance_id)return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultCandidateMismatch");auto old=read_private_shop_command_fact_sources(before,old_shops);if(!old.ok)return old;auto now=read_private_shop_command_fact_sources(c,shops);if(!now.ok)return now;std::map<StableId,const PrivateShopCommandFactSource*> prior;for(const auto& v:old.value)prior.emplace(v.id,&v);std::vector<PrivateShopCommandFactSource> fresh;std::size_t kept=0;
 for(const auto& v:now.value){const auto it=prior.find(v.id);if(it!=prior.end()){const auto& p=*it->second;if(p.kind!=v.kind||p.tick!=v.tick||!compare_boundary(p.stable_payload,v.stable_payload).exact())return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultOldSourceChanged",v.id);++kept;}else{auto n=number(c,v.id);if(!n||*n<before.state().next_instance_id||v.tick<a.tick||v.tick>b.tick)return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultNewSourceMismatch",v.id);fresh.push_back(v);}}
 if(kept!=prior.size())return fail<std::vector<PrivateShopCommandFactSource>>("PrivateShopResultOldSourceDeleted");return {true,std::move(fresh),{}};
}
}
