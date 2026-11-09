// 四个玩家司法办理结果只认实际commit_tail永久绑定，首条缓存裁剪也不退回“最新同tick”猜测。
#include "political_shop_licensing_result_bindings.hpp"
#include "political_shop_licensing_profile.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
struct Source {StableId actor,kind,id,first;Integer tick=0,number=0,first_number=0;};
Outcome<Source> missing(const StableId& id){return {false,{},common_failure("PoliticalShopLicensingPlayerSourceMismatch",id).error};}
// 只认真实四种终结业务；初始通缉册和单独clearance不能伪装成玩家提交。
Outcome<Source> source(const CommonStore& common,const PoliticalShopLicensingState& state,const StableId& id,const StableId& kind){
 Source out;out.id=id;out.first=id;out.kind=kind;StableId prefix;
 if(kind=="political_request_shop_permission"){
  const auto* p=state.permits.find(id);const auto* c=p?state.clearances.find(p->clearance_fact_id):nullptr;
  if(!p||p->id!=id||!c||c->id!=p->clearance_fact_id||c->tick!=p->tick)return missing(id);
  out.actor=c->request.actor_id;out.tick=p->tick;out.first=c->id;prefix="political_shop_permit";
 }else if(kind=="political_register_wanted"){
  const auto* p=state.warrants.find(id);if(!p||p->id!=id||!p->authority)return missing(id);out.actor=p->authority->actor_id;out.tick=p->tick;prefix="political_wanted";
 }else if(kind=="political_release_wanted"){
  const auto* p=state.releases.find(id);if(!p||p->id!=id)return missing(id);out.actor=p->authority.actor_id;out.tick=p->tick;prefix="political_wanted_release";
 }else if(kind=="political_revoke_shop_permit"){
  const auto* p=state.revocations.find(id);if(!p||p->id!=id)return missing(id);out.actor=p->authority.actor_id;out.tick=p->tick;prefix="political_shop_revocation";
 }else return missing(id);
 const auto* h=common.state().facts.find(id);auto n=organization_instance_sequence(id,prefix);
 if(!h||!n||!common.actor(out.actor)||out.tick<0||out.tick>common.tick())return missing(id);
 auto valid=validate_political_shop_licensing_fact_header(common,state,*h);if(!valid.ok)return {false,{},valid.error};out.number=*n;out.first_number=*n;
 if(out.first!=id){
  const auto* first=common.state().facts.find(out.first);auto fn=organization_instance_sequence(out.first,"political_shop_clearance");
  if(!first||!fn||*fn>=*n||*n-*fn!=1)return missing(id);valid=validate_political_shop_licensing_fact_header(common,state,*first);if(!valid.ok)return {false,{},valid.error};out.first_number=*fn;
  bool seen=false,ordered=false;for(const auto& row:common.state().facts.rows()){if(row.first==out.first)seen=true;if(row.first==id){ordered=seen;break;}}if(!ordered)return missing(id);
 }
 return {true,std::move(out),{}};
}
// 同候选差分只检查选中业务表的新增；完整法律验证仍是调用前后的强制门。
template<class T>Outcome<StableId> new_row(const OrderedTable<T>& before,const OrderedTable<T>& after){
 if(after.size()!=before.size()+1)return {false,{},common_failure("PoliticalShopLicensingNewPlayerSourceRequired").error};for(const auto& old:before.rows())if(!after.find(old.first))return {false,{},common_failure("PoliticalShopLicensingNewPlayerSourceRequired").error};StableId result;for(const auto& row:after.rows())if(!before.find(row.first)){if(!result.empty()||row.first!=row.second.id)return {false,{},common_failure("PoliticalShopLicensingNewPlayerSourceRequired").error};result=row.first;}return {true,std::move(result),{}};
}
Status permanent(const CommonStore& common,const PoliticalShopLicensingState& law,const PoliticalShopLicensingReceiptState& state){
 if(state.version!=1)return common_failure("UnsupportedPoliticalShopLicensingReceiptsVersion");
 std::set<StableId> results;std::set<std::pair<StableId,Integer>> requests;std::map<StableId,Integer> last_request;Integer previous_result=0,previous_revision=0;
 for(const auto& row:state.bindings.rows()){
  const auto& b=row.second;if(row.first!=b.source_fact_id)return common_failure("PoliticalShopLicensingBindingKeyMismatch",row.first);auto s=source(common,law,row.first,b.command_kind);if(!s.ok)return {false,{},s.error};auto result=organization_instance_sequence(b.result_id,"result");const auto* last=common.state().request_sequences.find(b.actor_id);
  if(b.actor_id!=s.value.actor||b.tick!=s.value.tick||!result||*result<=s.value.number||*result>=common.state().next_instance_id||s.value.first_number<=previous_result||b.request_sequence<1||b.request_sequence>2147483647||!last||b.request_sequence>*last||b.request_sequence<=last_request[b.actor_id]||b.result_revision<1||b.result_revision>common.revision()||b.result_revision<=previous_revision||!results.insert(b.result_id).second||!requests.emplace(b.actor_id,b.request_sequence).second)return common_failure("PoliticalShopLicensingPlayerBindingMismatch",row.first);
  Integer histories=0;for(const auto& h:common.state().history)if(h.result_id==b.result_id){++histories;if(h.actor_id!=b.actor_id||h.kind!=b.command_kind||h.tick!=b.tick)return common_failure("PoliticalShopLicensingBindingHistoryMismatch",row.first);}if(histories>1)return common_failure("PoliticalShopLicensingBindingHistoryMismatch",row.first);
  previous_result=*result;previous_revision=b.result_revision;last_request[b.actor_id]=b.request_sequence;
 }
 return common_success();
}
}
Outcome<StableId> political_shop_licensing_new_player_source(const PoliticalShopLicensingState& before,const PoliticalShopLicensingState& after,const StableId& actor,const StableId& kind,Integer tick){
 const auto fail=[]()->Outcome<StableId>{return {false,{},common_failure("PoliticalShopLicensingNewPlayerSourceRequired").error};};if(before.version!=1||after.version!=1||actor.empty()||tick<0)return fail();
 Outcome<StableId> id;StableId who;Integer actual=-1;
 if(kind=="political_request_shop_permission"){
  id=new_row(before.permits,after.permits);auto clear=new_row(before.clearances,after.clearances);if(!id.ok||!clear.ok)return fail();const auto* p=after.permits.find(id.value);const auto* c=after.clearances.find(clear.value);if(!p||!c||p->clearance_fact_id!=c->id||p->tick!=c->tick)return fail();who=c->request.actor_id;actual=p->tick;
 }else if(kind=="political_register_wanted"){
  id=new_row(before.warrants,after.warrants);if(!id.ok)return id;const auto* p=after.warrants.find(id.value);if(!p->authority)return fail();who=p->authority->actor_id;actual=p->tick;
 }else if(kind=="political_release_wanted"){
  id=new_row(before.releases,after.releases);if(!id.ok)return id;const auto* p=after.releases.find(id.value);who=p->authority.actor_id;actual=p->tick;
 }else if(kind=="political_revoke_shop_permit"){
  id=new_row(before.revocations,after.revocations);if(!id.ok)return id;const auto* p=after.revocations.find(id.value);who=p->authority.actor_id;actual=p->tick;
 }else return fail();return who==actor&&actual==tick?id:fail();
}
Status bind_political_shop_licensing_player_result(const CommonStore& common,const PoliticalShopLicensingState& law,PoliticalShopLicensingReceiptState& state,const StableId& id,const StableId& actor,Integer request,const CommitResult& result,const std::function<bool(const CommitResult&)>& is_done){
 const auto* cached=common.state().results.find(actor+":"+std::to_string(request));const auto* sequence=common.state().request_sequences.find(actor);
 if(state.bindings.find(id)||!cached||cached->get()!=&result||!sequence||*sequence!=request||result.tick!=common.tick()||result.revision!=common.revision()||!is_done||!is_done(result)||common.state().history.empty())return common_failure("PoliticalShopLicensingActualPlayerResultRequired",id);
 const auto& h=common.state().history.back();if(h.actor_id!=actor||h.result_id!=result.result_id||h.tick!=result.tick||h.kind!=result.kind)return common_failure("PoliticalShopLicensingActualPlayerResultRequired",id);
 auto s=source(common,law,id,result.kind);if(!s.ok)return {false,{},s.error};if(s.value.actor!=actor||s.value.tick!=result.tick)return common_failure("PoliticalShopLicensingActualPlayerResultRequired",id);
 // 先在窄副本核所有绑定，唯一发布点之后不再失败；不触Common和法律原账。
 auto next=state;next.bindings.insert(id,{id,actor,result.kind,result.result_id,result.tick,request,result.revision});auto valid=permanent(common,law,next);if(!valid.ok)return valid;state=std::move(next);return common_success();
}
Status validate_political_shop_licensing_player_results(const CommonStore& common,const PoliticalShopLicensingState& law,const PoliticalShopLicensingReceiptState& state,const std::function<bool(const CommitResult&)>& is_done){
 auto valid=permanent(common,law,state);if(!valid.ok)return valid;std::set<StableId> used;
 for(const auto& row:common.state().results.rows()){
  if(!row.second||!political_shop_licensing_command_kind(row.second->kind))continue;const auto& r=*row.second;const auto split=row.first.rfind(':');if(split==StableId::npos||!is_done||!is_done(r))return common_failure("PoliticalShopLicensingExactReceiptRequired",row.first);const auto actor=row.first.substr(0,split);auto request=organization_instance_sequence(row.first,actor);if(!request)return common_failure("PoliticalShopLicensingExactReceiptRequired",row.first);
  const PoliticalShopLicensingPlayerResult* bound=nullptr;for(const auto& b:state.bindings.rows())if(b.second.result_id==r.result_id){if(bound)return common_failure("PoliticalShopLicensingExactReceiptRequired",row.first);bound=&b.second;}
  if(!bound||bound->actor_id!=actor||bound->request_sequence!=*request||bound->command_kind!=r.kind||bound->tick!=r.tick||bound->result_revision!=r.revision||!used.insert(bound->source_fact_id).second)return common_failure("PoliticalShopLicensingExactReceiptRequired",row.first);
  const HistoryRecord* matched=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* prior=nullptr;for(const auto& h:common.state().history){if(h.result_id==r.result_id){if(matched)return common_failure("PoliticalShopLicensingBindingHistoryMismatch",row.first);matched=&h;lower=prior;}prior=&h;}
  if(!matched||matched->kind!=r.kind||matched->actor_id!=actor||matched->tick!=r.tick)return common_failure("PoliticalShopLicensingBindingHistoryMismatch",row.first);
  auto s=source(common,law,bound->source_fact_id,r.kind);if(!s.ok)return {false,{},s.error};if(lower){auto n=organization_instance_sequence(lower->result_id,"result");if(!n||*n>=s.value.first_number)return common_failure("PoliticalShopLicensingBindingAllocationMismatch",row.first);}
 }
 return common_success();
}
}
