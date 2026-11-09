// 不使用存在位偷加舟战；旧作者、旧bank和旧回执仍由段1外层合同严格限定。
#include "naval_codec.hpp"
#include "naval_json_guard.hpp"
#include "naval_asset_codec.hpp"
#include "naval_repair_codec.hpp"
#include "naval_repair_charges_codec.hpp"
#include "naval_repair.hpp"
#include "naval_navigation.hpp"
#include "naval_navigation_codec.hpp"
#include "naval_gunnery_codec.hpp"
#include "naval_gunnery_rules.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident::naval_codec {
namespace {
// 未启用战斗时连孤立历史事实也拒绝，不只检查当前打开的战场。
bool no_battle(const NavalBattleState& b){return b.battles.empty()&&b.contacts.empty()&&b.plans.empty()&&b.rounds.empty()&&b.captures.empty()&&b.declines.empty()&&b.continuations.empty()&&b.field_order.empty();}
Status shape(const FieldOrder& order,bool repairs,bool charges,bool navigation,bool gunnery){if(order.empty())return common_success();const std::set<StableId> keys=gunnery?std::set<StableId>{"assets","battle","repairs","repair_charges","navigation","gunnery"}:navigation?std::set<StableId>{"assets","battle","repairs","repair_charges","navigation"}:charges?std::set<StableId>{"assets","battle","repairs","repair_charges"}:repairs?std::set<StableId>{"assets","battle","repairs"}:std::set<StableId>{"assets","battle"};return order.size()==keys.size()&&std::set<StableId>(order.begin(),order.end())==keys?common_success():common_failure("InvalidNavalFieldOrder");}
bool no_charges(const NavalRepairChargeState& s){return s.charges.empty()&&s.field_order.empty();}
bool no_repairs(const NavalRepairState& s){return s.starts.empty()&&s.results.empty()&&s.field_order.empty();}
bool no_navigation(const NavalNavigationState& s){return s.voyages.empty()&&s.steps.empty()&&s.field_order.empty();}
bool gunnery(const NavalCatalog& c){return c.combat_policy&&bool(c.combat_policy->combat.gunnery);}
bool no_gunnery(const NavalGunneryState& s){return s.preparations.empty()&&s.field_order.empty();}
Status policy(const NavalCatalog& c){if(gunnery(c)){if(!c.navigation_policy)return common_failure("NavalGunneryNavigationRequired");auto checked=validate_naval_gunnery_policy(*c.combat_policy->combat.gunnery,c);if(!checked.ok)return checked;}if(c.navigation_policy){auto checked=validate_naval_navigation_policy(*c.navigation_policy,c);if(!checked.ok)return checked;}if(c.repair_charge_policy&&(!c.repair_policy||!c.combat_policy||c.repair_charge_policy->profile!="public_dock_fees_v1"||c.repair_charge_policy->services.empty()||c.repair_charge_policy->services.size()>256))return common_failure("NavalRepairChargeProfileRequired");if(c.repair_policy){auto check=validate_naval_repair_policy(c,nullptr);if(!check.ok)return check;}if(c.combat_policy&&c.combat_policy->kind!="boarding_ram_v1")return common_failure("InvalidNavalBattlePolicy");return common_success();}
}
// 同一feature下仍由显式政策选择独立段版本，公共Common回执也须使用该上下文。
Integer version(const NavalCatalog& c){return gunnery(c)?6:c.navigation_policy?5:c.repair_charge_policy?4:c.repair_policy?3:c.combat_policy?2:1;}
Status emit_fields(ProjectionWriter& w,const NavalState& s,const NavalCatalog& c){
 if(!gunnery(c)&&!no_gunnery(s.gunnery))return common_failure("NavalGunneryProfileRequired","naval");
 if(!c.navigation_policy&&!no_navigation(s.navigation))return common_failure("NavalNavigationProfileRequired","naval");
 if(!c.repair_charge_policy&&!no_charges(s.repair_charges))return common_failure("NavalRepairChargeProfileRequired","naval");
 if(!c.repair_policy&&!no_repairs(s.repairs))return common_failure("NavalRepairProfileRequired","naval");
 auto checked=policy(c);if(!checked.ok)return checked;
 if(!c.combat_policy){if(!no_battle(s.battle)||!s.field_order.empty())return common_failure("NavalCombatProfileRequired","naval");return naval_asset_codec::emit_fields(w,s);}
 checked=validate_naval_assets(s,c,true);if(!checked.ok)return checked;
 checked=shape(s.field_order,bool(c.repair_policy),bool(c.repair_charge_policy),bool(c.navigation_policy),gunnery(c));if(!checked.ok)return common_failure(checked.error.original.code,"naval");
 w.object("assets",[&](auto& child){return naval_asset_codec::emit_fields(child,s);});
 w.object("battle",[&](auto& child){return naval_battle_codec::emit_fields(child,s.battle);});
 if(c.repair_policy)w.object("repairs",[&](auto& child){return naval_repair_codec::emit_state_fields(child,s.repairs,bool(c.repair_charge_policy));});
 if(c.repair_charge_policy)w.object("repair_charges",[&](auto& child){return naval_repair_charges_codec::emit_fields(child,s.repair_charges);});
 if(c.navigation_policy)w.object("navigation",[&](auto& child){return naval_navigation_codec::emit_fields(child,s.navigation);});
 if(gunnery(c))w.object("gunnery",[&](auto& child){return naval_gunnery_codec::emit_fields(child,s.gunnery);});
 w.order_fields(s.field_order);return w.status();
}
BoundaryValue encode(const NavalState& s,const NavalCatalog& c){BoundaryValue out;BoundaryProjectionWriter sink(out);auto checked=sink.object("",[&](auto& w){return emit_fields(w,s,c);});if(checked.ok)checked=sink.finish();if(!checked.ok)boundary::fail(checked.error.original.code,checked.error.original.path);return out;}
Outcome<NavalState> decode(const BoundaryValue& v,const NavalCatalog& c){try{auto checked=policy(c);if(!checked.ok)return {false,{},checked.error};NavalState out;if(!c.combat_policy){auto assets=naval_asset_codec::decode(v,c);if(!assets.ok)return {false,{},assets.error};static_cast<NavalAssetState&>(out)=std::move(assets.value);return {true,std::move(out),{}};}
 checked=validate_naval_json_unique_keys(v,"naval");if(!checked.ok)return {false,{},checked.error};if(gunnery(c))boundary::fields(v,{"assets","battle","repairs","repair_charges","navigation","gunnery"},{},"naval");else if(c.navigation_policy)boundary::fields(v,{"assets","battle","repairs","repair_charges","navigation"},{},"naval");else if(c.repair_charge_policy)boundary::fields(v,{"assets","battle","repairs","repair_charges"},{},"naval");else if(c.repair_policy)boundary::fields(v,{"assets","battle","repairs"},{},"naval");else boundary::fields(v,{"assets","battle"},{},"naval");auto assets=naval_asset_codec::decode(boundary::field(v,"assets","naval"),c,true);if(!assets.ok)return {false,{},assets.error};auto battle=naval_battle_codec::decode(boundary::field(v,"battle","naval"),true);if(!battle.ok)return {false,{},battle.error};static_cast<NavalAssetState&>(out)=std::move(assets.value);out.battle=std::move(battle.value);if(c.repair_policy){auto repairs=naval_repair_codec::decode(boundary::field(v,"repairs","naval"),bool(c.repair_charge_policy));if(!repairs.ok)return {false,{},repairs.error};out.repairs=std::move(repairs.value);}if(c.repair_charge_policy){auto charges=naval_repair_charges_codec::decode(boundary::field(v,"repair_charges","naval"));if(!charges.ok)return {false,{},charges.error};out.repair_charges=std::move(charges.value);}if(c.navigation_policy){auto navigation=naval_navigation_codec::decode(boundary::field(v,"navigation","naval"));if(!navigation.ok)return {false,{},navigation.error};out.navigation=std::move(navigation.value);}if(gunnery(c)){auto prepared=naval_gunnery_codec::decode(boundary::field(v,"gunnery","naval"));if(!prepared.ok)return {false,{},prepared.error};out.gunnery=std::move(prepared.value);}out.field_order=boundary::order(v);return {true,std::move(out),{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 旧段直接调用原访客，不写新标记/存在位；新段先保原资产字段，再接固定舟战事实格式。
Status visit(CodecVisitor& codec,NavalState& state,const NavalCatalog& catalog){if((codec.naval_codec_version==6)!=gunnery(catalog)||(!gunnery(catalog)&&!no_gunnery(state.gunnery)))return codec.reject("NavalGunneryProfileRequired");if(!catalog.navigation_policy&&!no_navigation(state.navigation))return codec.reject("NavalNavigationProfileRequired");if(!catalog.repair_charge_policy&&!no_charges(state.repair_charges))return codec.reject("NavalRepairChargeProfileRequired");if(!catalog.repair_policy&&!no_repairs(state.repairs))return codec.reject("NavalRepairProfileRequired");auto checked=policy(catalog);if(!checked.ok)return codec.reject(checked.error.original.code);if(!catalog.combat_policy){if(!no_battle(state.battle)||!state.field_order.empty())return codec.reject("NavalCombatProfileRequired");return naval_asset_codec::visit(codec,state,catalog);}
 checked=naval_asset_codec::visit(codec,state,catalog,true);if(!checked.ok)return checked;checked=naval_battle_codec::visit(codec,state.battle,true);if(!checked.ok)return checked;if(catalog.repair_policy){checked=naval_repair_codec::visit(codec,state.repairs,bool(catalog.repair_charge_policy));if(!checked.ok)return checked;}if(catalog.repair_charge_policy){checked=naval_repair_charges_codec::visit(codec,state.repair_charges);if(!checked.ok)return checked;}if(catalog.navigation_policy){checked=naval_navigation_codec::visit(codec,state.navigation);if(!checked.ok)return checked;}if(gunnery(catalog)){checked=naval_gunnery_codec::visit(codec,state.gunnery);if(!checked.ok)return checked;}checked=common_checkpoint::strings(codec,"field_order",state.field_order);if(!checked.ok)return checked;checked=shape(state.field_order,bool(catalog.repair_policy),bool(catalog.repair_charge_policy),bool(catalog.navigation_policy),gunnery(catalog));return checked.ok?codec.status():codec.reject(checked.error.original.code);
}
}
