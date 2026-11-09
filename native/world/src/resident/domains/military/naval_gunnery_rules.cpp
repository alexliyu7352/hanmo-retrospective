// 普通舰载攻击不生成接管权：船体归零仍由原失能/撤退/接舷与显式投降链处理。
#include "military_attack_damage_core.hpp"
#include "naval_gunnery_rules.hpp"
#include "naval_gunnery_source.hpp"
#include "naval_commander.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
const NavalBattleBinding* binding(const NavalTacticalFrame& f,const StableId& id){for(const auto& b:f.bindings)if(b.unit_id==id)return &b;return nullptr;}
const NavalGunneryCaptain* captain(const NavalGunneryPreparation& p,const StableId& id){for(const auto& c:p.captains)if(c.unit_id==id)return &c;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
}
// 启用后的每种可入战原船型都必须有唯一武器定义，不能在AI报价时临时补射程。
Status validate_naval_gunnery_policy(const NavalGunneryPolicy& p,const NavalAssetCatalog& catalog){if(p.profile!="entry_naval_skill_v1"||p.weapons.size()!=catalog.hulls.size()||p.weapons.empty())return common_failure("InvalidNavalGunneryPolicy");std::set<StableId> ids;for(const auto& w:p.weapons)if(!find(catalog.hulls,w.hull_id)||!ids.insert(w.hull_id).second||w.minimum_range<1||w.maximum_range<w.minimum_range||w.maximum_range>16)return common_failure("InvalidNavalGunneryWeapon",w.hull_id);return common_success();}
// 基础船体压制严格按10+3×入战水战级，人员命中/侧击/护甲由原軍旅纯报价计算。
// 本场captor可沿原capture/continue换入敌船：技艺仍属原unit/captain，武器改读当前已验证binding船型。
Outcome<NavalGunneryQuote> preview_naval_gunnery(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalGunneryPolicy& policy,const MilitaryBattleCatalog& military,const NavalGunneryPreparation& prepared,const NavalTacticalFrame& frame,const StableId& id,const StableId& target){
 auto checked=validate_naval_gunnery_policy(policy,ships);if(!checked.ok)return {false,{},checked.error};const auto* board=find(military.battlefields,frame.battlefield_id);if(!board)return fail<NavalGunneryQuote>("UnknownNavalBattlefield",frame.battlefield_id);checked=validate_naval_tactical_frame(assets,ships,*board,frame);if(!checked.ok)return {false,{},checked.error};checked=validate_naval_gunnery_frame_source(prepared,frame);if(!checked.ok)return {false,{},checked.error};
 const auto* attacker=unit(frame,id);const auto* defender=unit(frame,target);const auto* link=binding(frame,id);const auto* skill=captain(prepared,id);const auto* vessel=link?find(assets.vessels,link->vessel_id):nullptr;
 if(prepared.battle_id!=frame.id||prepared.contact_source_id!=frame.source_id||!attacker||!link||!skill||!vessel||skill->fleet_id!=link->fleet_id||skill->army_id!=attacker->army_id||skill->captain_actor_id!=attacker->commander_actor_id)return fail<NavalGunneryQuote>("NavalGunneryBattleSourceMismatch",id);
 if(attacker->withdrawn||attacker->fit.empty()||attacker->morale<=0||naval_commander_loss(*attacker)==2)return fail<NavalGunneryQuote>("NavalGunneryCrewRequired",id);if(!defender||defender->withdrawn||attacker->side==defender->side)return fail<NavalGunneryQuote>("NavalGunneryTargetUnavailable",target);
 const NavalGunneryWeapon* weapon=nullptr;for(const auto& w:policy.weapons)if(w.hull_id==vessel->hull_id)weapon=&w;if(!weapon)return fail<NavalGunneryQuote>("NavalGunneryWeaponRequired",vessel->id);
 auto personnel=preview_military_afloat_attack(*attacker,*defender,military,frame.battlefield_id,frame.round,weapon->minimum_range,weapon->maximum_range);if(!personnel.ok)return {false,{},personnel.error};return {true,{prepared.id,skill->captain_actor_id,skill->level,10+3*skill->level,personnel.value},{}};
}
// 所有攻击先看同一移动后快照；已击破对手本轮仍能还击，不能因容器顺序抢先消除其意图。
Outcome<NavalGunneryResult> resolve_naval_gunnery(const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalGunneryPolicy& policy,const MilitaryBattleCatalog& military,const NavalGunneryPreparation& prepared,const NavalTacticalFrame& frame,const std::vector<std::pair<StableId,StableId>>& requests,const StableId& seed,const StableId& state,const MilitaryBattleRandom& random){
 auto checked=validate_naval_gunnery_policy(policy,ships);if(!checked.ok)return {false,{},checked.error};NavalGunneryResult out;out.rng_seed=seed;out.rng_state=state;auto ordered=requests;std::sort(ordered.begin(),ordered.end());StableId previous;
 for(const auto& request:ordered){if(request.first.empty()||request.first==previous)return fail<NavalGunneryResult>("DuplicateNavalGunneryOrder",request.first);previous=request.first;
  auto quote=preview_naval_gunnery(assets,ships,policy,military,prepared,frame,request.first,request.second);if(!quote.ok){if(quote.error.original.code=="MilitaryTargetOutOfRange"||quote.error.original.code=="MilitarySightBlocked"||quote.error.original.code=="NavalGunneryTargetUnavailable"){out.events.push_back({"hull_attack",request.first,request.second,"NavalGunneryOutOfReach",0,0,0,0});continue;}return {false,{},quote.error};}
  const auto* defender=unit(frame,request.second);const auto* link=binding(frame,request.second);const auto* vessel=link?find(assets.vessels,link->vessel_id):nullptr;if(!defender||!vessel)return fail<NavalGunneryResult>("NavalGunneryTargetSourceRequired",request.second);if(!random)return fail<NavalGunneryResult>("MilitaryRandomServiceRequired");auto draw=random(out.rng_seed,out.rng_state,1,100);if(!draw.ok)return {false,{},draw.error};if(draw.value<1||draw.value>100)return fail<NavalGunneryResult>("InvalidMilitaryRandomDraw");
  const auto& q=quote.value;const auto expected=original_military_casualty_expectation(q.personnel.potential,q.personnel.hit_chance);Integer loss=original_military_sampled_casualties(expected,draw.value);loss=std::min<Integer>(loss,std::max<Integer>(0,Integer(defender->fit.size())-out.casualties[request.second]));const Integer damage=draw.value<=q.personnel.hit_chance?std::min<Integer>(q.hull_damage,std::max<Integer>(0,vessel->integrity-out.hull_damage[vessel->id])):0;
  out.hull_damage[vessel->id]+=damage;out.casualties[request.second]+=loss;if(loss>0)out.morale[request.second]+=original_military_casualty_morale_loss(loss,Integer(defender->fit.size()),false,q.personnel.flank);out.events.push_back({"hull_attack",request.first,request.second,(damage||loss)?"NavalGunneryHit":"NavalGunneryMiss",loss,damage,0,q.personnel.hit_chance});
 }return {true,std::move(out),{}};
}
}
