#include "resident/domains/military/military_model.hpp"
// 原子边界在完整World候选；本叶只写同候选Common身份/事实与Naval唯一占用。
#include "naval_commands.hpp"
#include "naval_repair.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
namespace {
// 向量实体只读查找，不缓存跨提交指针。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 业务键有界且不可包含控制符，字段顺序不改变其语义。
bool key_valid(const StableId& key){return naval_request_key_valid(key);}
// 同键必须相同的完整意图，不通过数组重排偷偷改动绑定。
bool same_assignments(const std::vector<NavalVesselAssignment>& a,const std::vector<NavalVesselAssignment>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(std::tie(a[i].vessel_id,a[i].unit_id,a[i].passenger_ids,a[i].prisoner_ids,a[i].extra_inventory_ids)!=std::tie(b[i].vessel_id,b[i].unit_id,b[i].passenger_ids,b[i].prisoner_ids,b[i].extra_inventory_ids))return false;return true;}
// 回执只从已经持久的事实字段产生，重放不再读取当前时刻或重新分配ID。
NavalCommandReceipt receipt(const NavalFleetRecord& f,bool release,const StableId& formed_actor={}){return {release?"naval_disband":"naval_form",release?f.release_request_key:f.request_key,release?f.release_source_id:f.source_id,f.id,f.army_id,release?f.release_actor_id:(formed_actor.empty()?f.commander_actor_id:formed_actor),release?f.released_tick:f.created_tick};}
// 拒绝永远不携带部分写入结果。
template<class T>Outcome<T> failure(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
}
// 活动指挥可以换任，原形成者必须仍由同一个成功ArmyForm事实唯一确认。
Outcome<StableId> naval_formation_actor(const MilitaryState& s,const NavalFleetRecord& fleet){const auto* f=find(s.action_facts,fleet.formation_source);const auto* t=f?std::get_if<ArmyFormationTerms>(&f->terms):nullptr;if(!f||!t||f->command!="army_form"||f->phase!="completed"||!f->success||!f->result||f->result->army_id!=fleet.army_id||f->actor_id!=t->actor_id||f->completed_tick<0||f->completed_tick>fleet.created_tick)return failure<StableId>("NavalFormationHistoryMismatch",fleet.id);return {true,t->actor_id,{}};}
// 先查永久业务键再核新意图；既有成功结果不因后来撤职或返营而失去可重放性。
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalAssetState& n,const NavalAssetCatalog& d,const StableId& actor,const NavalCommand& command){return preview_naval_command(c,s,cat,a,n,d,actor,command,false);}
// 战斗上下文只决定合法历史形状，原军令/载重/泊地权限继续原路径。
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalAssetState& n,const NavalAssetCatalog& d,const StableId& actor,const NavalCommand& command,bool combat_enabled){
 const auto* form=std::get_if<NavalForm>(&command);const auto* release=std::get_if<NavalDisband>(&command);const auto& key=form?form->request_key:release->request_key;if(!key_valid(key))return failure<NavalCommandQuote>("InvalidNavalRequestKey");auto valid=validate_naval_assets(n,d,combat_enabled);if(!valid.ok)return {false,{},valid.error};
 for(const auto& f:n.fleets){auto original=naval_formation_actor(s,f);if(!original.ok)return {false,{},original.error};bool formed=original.value==actor&&f.request_key==key,freed=f.status=="released"&&f.release_actor_id==actor&&f.release_request_key==key;if(!formed&&!freed)continue;if((formed&&(!form||f.army_id!=form->formation.army_id||!same_assignments(f.assignments,form->formation.assignments)))||(freed&&(!release||release->fleet_id!=f.id)))return failure<NavalCommandQuote>("NavalIdempotencyConflict",key);NavalCommandQuote q;q.replayed=true;q.receipt=receipt(f,freed,original.value);return {true,std::move(q),{}};}
 if(form){auto observed=observe_naval_formation(c,s,cat,a,n,d,actor,form->formation,combat_enabled);if(!observed.ok)return {false,{},observed.error};auto quoted=preview_naval_formation(n,d,observed.value,form->formation,false,combat_enabled);if(!quoted.ok)return {false,{},quoted.error};NavalCommandQuote q;q.berth_id=observed.value.berth_id;q.formation=std::move(quoted.value);return {true,std::move(q),{}};}
 const auto* f=find(n.fleets,release->fleet_id);if(!f||f->status!="active")return failure<NavalCommandQuote>("NavalFleetAuthorityRequired",release->fleet_id);auto berth=preview_naval_release(c,s,cat,a,d,*f,actor);if(!berth.ok)return {false,{},berth.error};NavalCommandQuote q;q.berth_id=berth.value.berth_id;q.release=berth.value;return {true,std::move(q),{}};
}
// 世界候选统一提交前写一次事实；原钱货、船损、人物位置和tick完全不由此命令改写。
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,NavalAssetState& n,const NavalAssetCatalog& d,const StableId& actor,const NavalCommand& command){return apply_naval_command(c,s,cat,a,n,d,actor,command,false);}
// 基类引用只替换实船部分，不丢弃派生 Naval2 的永久战史。
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,NavalAssetState& n,const NavalAssetCatalog& d,const StableId& actor,const NavalCommand& command,bool combat_enabled){
 auto quote=preview_naval_command(c,s,cat,a,n,d,actor,command,combat_enabled);if(!quote.ok)return {false,{},quote.error};if(quote.value.replayed)return {true,quote.value.receipt,{}};
 if(const auto* form=std::get_if<NavalForm>(&command)){auto observed=observe_naval_formation(c,s,cat,a,n,d,actor,form->formation,combat_enabled);if(!observed.ok)return {false,{},observed.error};auto fleet=c.next_id("fleet");if(!fleet.ok)return {false,{},fleet.error};auto source=c.next_id("naval_action");if(!source.ok)return {false,{},source.error};auto next=commit_naval_formation(n,d,observed.value,form->formation,fleet.value,source.value,combat_enabled);if(!next.ok)return {false,{},next.error};next.value.fleets.back().request_key=form->request_key;auto appended=c.append_fact({source.value,"naval_form","military_naval"});if(!appended.ok)return {false,{},appended.error};auto result=receipt(next.value.fleets.back(),false);n=std::move(next.value);return {true,std::move(result),{}};}
 const auto& release=std::get<NavalDisband>(command);auto source=c.next_id("naval_action");if(!source.ok)return {false,{},source.error};auto next=n;NavalCommandReceipt result;for(auto& fleet:next.fleets)if(fleet.id==release.fleet_id){fleet.status="released";fleet.release_request_key=release.request_key;fleet.release_source_id=source.value;fleet.release_berth_id=quote.value.berth_id;fleet.release_actor_id=actor;fleet.release_authority_kind=quote.value.release->authority_kind;fleet.release_authority_id=quote.value.release->authority_id;fleet.release_authority_source=quote.value.release->authority_source;fleet.released_tick=c.tick();result=receipt(fleet,true);}for(auto& v:next.vessels)if(v.fleet_id==release.fleet_id){v.fleet_id.clear();v.berth_id=quote.value.berth_id;v.afloat_location.reset();}auto valid=validate_naval_assets(next,d,combat_enabled);if(!valid.ok)return {false,{},valid.error};auto appended=c.append_fact({source.value,"naval_disband","military_naval"});if(!appended.ok)return {false,{},appended.error};n=std::move(next);return {true,std::move(result),{}};
}
// 资产段1没有战史，逐字保原入口；段2只在已成功同键重放时逆取原名单。
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalState& n,const NavalCatalog& d,const StableId& actor,const NavalCommand& command){
 if(d.combat_policy)if(const auto* form=std::get_if<NavalForm>(&command))for(const auto& fleet:n.fleets){auto original_actor=naval_formation_actor(s,fleet);if(!original_actor.ok)return {false,{},original_actor.error};if(original_actor.value!=actor||fleet.request_key!=form->request_key)continue;
  auto valid=validate_naval_assets(n,d,true);if(!valid.ok)return {false,{},valid.error};auto original=fleet.assignments;
  for(auto it=n.battle.captures.rbegin();it!=n.battle.captures.rend();++it){const auto* after=it->to_fleet_id==fleet.id?&it->after_assignments:it->from_fleet_id==fleet.id?&it->from_after_assignments:nullptr;const auto* before=it->to_fleet_id==fleet.id?&it->before_assignments:it->from_fleet_id==fleet.id?&it->from_before_assignments:nullptr;if(after){if(!same_assignments(original,*after))return failure<NavalCommandQuote>("NavalCaptureManifestPredecessorMismatch",it->id);original=*before;}}
  if(fleet.army_id!=form->formation.army_id||!same_assignments(original,form->formation.assignments))return failure<NavalCommandQuote>("NavalIdempotencyConflict",form->request_key);NavalCommandQuote out;out.replayed=true;out.receipt=receipt(fleet,false,original_actor.value);return {true,std::move(out),{}};
 }
 auto result=preview_naval_command(c,s,cat,a,static_cast<const NavalAssetState&>(n),static_cast<const NavalAssetCatalog&>(d),actor,command,bool(d.combat_policy));
 if(!result.ok&&result.error.original.code=="NavalVesselDisabled"&&d.repair_policy)if(const auto* form=std::get_if<NavalForm>(&command)){auto observed=observe_naval_formation(c,s,cat,a,n,d,actor,form->formation,true);if(!observed.ok)return {false,{},observed.error};auto quote=preview_naval_dock_repair_formation(n,d,observed.value,form->formation);if(!quote.ok)return {false,{},quote.error};NavalCommandQuote out;out.berth_id=observed.value.berth_id;out.formation=std::move(quote.value);return {true,std::move(out),{}};}
 return result;
}
// 旧来源回放不分配新身份；首次执行仍由原唯一编舰写口负责，不复制载重算法。
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,NavalState& n,const NavalCatalog& d,const StableId& actor,const NavalCommand& command){
 auto q=preview_naval_command(c,s,cat,a,n,d,actor,command);if(!q.ok)return {false,{},q.error};if(q.value.replayed)return {true,q.value.receipt,{}};
 if(d.repair_policy)if(const auto* form=std::get_if<NavalForm>(&command)){bool disabled=false;for(const auto& binding:form->formation.assignments){const auto* v=find(n.vessels,binding.vessel_id);disabled=disabled||(v&&v->integrity==0);}if(disabled){auto observed=observe_naval_formation(c,s,cat,a,n,d,actor,form->formation,true);if(!observed.ok)return {false,{},observed.error};auto fleet=c.next_id("fleet");if(!fleet.ok)return {false,{},fleet.error};auto source=c.next_id("naval_action");if(!source.ok)return {false,{},source.error};auto next=commit_naval_dock_repair_formation(n,d,observed.value,form->formation,fleet.value,source.value);if(!next.ok)return {false,{},next.error};next.value.fleets.back().request_key=form->request_key;auto appended=c.append_fact({source.value,"naval_form","military_naval"});if(!appended.ok)return {false,{},appended.error};auto result=receipt(next.value.fleets.back(),false);static_cast<NavalAssetState&>(n)=std::move(next.value);return {true,std::move(result),{}};}}
 return apply_naval_command(c,s,cat,a,static_cast<NavalAssetState&>(n),static_cast<const NavalAssetCatalog&>(d),actor,command,bool(d.combat_policy));
}

}
