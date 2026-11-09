// 永久业务重放先核原命令意图，普通新编解编仍复用唯一权限/泊地/载荷和零时长写口。
#include "naval_postcombat_commands.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
bool same(const std::vector<NavalVesselAssignment>& a,const std::vector<NavalVesselAssignment>& b){if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(a[n].unit_id!=b[n].unit_id||a[n].vessel_id!=b[n].vessel_id||a[n].passenger_ids!=b[n].passenger_ids||a[n].prisoner_ids!=b[n].prisoner_ids||a[n].extra_inventory_ids!=b[n].extra_inventory_ids)return false;return true;}
}
// 完整世界已验证资产历史；这里只把这条舰队按真实捕获链回溯到原形成请求。
Outcome<NavalCommandQuote> preview_naval_postcombat_command(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalCommand& command){
 if(!ships.combat_policy)return {false,{},common_failure("NavalCombatProfileRequired").error};if(const auto* form=std::get_if<NavalForm>(&command))for(const auto& fleet:naval.fleets)if(fleet.commander_actor_id==actor&&fleet.request_key==form->request_key){auto original=fleet.assignments;for(auto at=naval.battle.captures.rbegin();at!=naval.battle.captures.rend();++at){const auto* before=at->to_fleet_id==fleet.id?&at->before_assignments:at->from_fleet_id==fleet.id?&at->from_before_assignments:nullptr;const auto* after=at->to_fleet_id==fleet.id?&at->after_assignments:at->from_fleet_id==fleet.id?&at->from_after_assignments:nullptr;if(before){if(!same(original,*after))return {false,{},common_failure("NavalCaptureManifestPredecessorMismatch",at->id).error};original=*before;}}if(fleet.army_id!=form->formation.army_id||!same(original,form->formation.assignments))return {false,{},common_failure("NavalIdempotencyConflict",form->request_key).error};NavalCommandQuote quote;quote.replayed=true;quote.receipt={"naval_form",fleet.request_key,fleet.source_id,fleet.id,fleet.army_id,fleet.commander_actor_id,fleet.created_tick,{}};return {true,std::move(quote),{}};}
 return preview_naval_command(common,military,commands,access,naval,ships,actor,command,true);
}
// 不把战斗字段切片丢掉；旧asset基类引用只改原编队/船只，Naval2永久战史保留。
Outcome<NavalCommandReceipt> apply_naval_postcombat_command(CommonWrite& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalCommand& command){auto quote=preview_naval_postcombat_command(common,military,commands,access,naval,ships,actor,command);if(!quote.ok)return {false,{},quote.error};if(quote.value.replayed)return {true,quote.value.receipt,{}};return apply_naval_command(common,military,commands,access,naval,ships,actor,command,true);}
}
