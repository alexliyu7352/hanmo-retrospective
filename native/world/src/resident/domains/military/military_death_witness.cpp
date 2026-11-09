// 读取原具名死亡、真实参战单位与当轮分类；不从今天的alive或同军名册回填历史见闻。
#include "military_death_witness.hpp"
#include "naval_casualties.hpp"
#include "naval_commander.hpp"
#include "military_commander.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
template<class T>Outcome<T> fail(const StableId& id){return {false,{},common_failure("MilitaryDeathWitnessSourceMismatch",id).error};}
bool precedes(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto first=std::find(order.begin(),order.end(),a),last=std::find(order.begin(),order.end(),b);return first!=order.end()&&last!=order.end()&&first<last;}
// 收据原分类只能证明本轮后仍在场者；已经战死、被俘、逃散者不被推定看到全部后果。
bool survivor(const MilitaryRosterReceipt& r,const StableId& actor){return has(r.fit,actor)||has(r.wounded,actor);}
}
Outcome<std::optional<MilitaryDeathWitness>> query_military_actor_death_witness(const CommonStore& c,const MilitaryState& m,const NavalState* naval,const StableId& observer,const StableId& dead,const StableId& source){
 using Result=std::optional<MilitaryDeathWitness>;const auto* person=c.state().actors.find(observer);if(observer.empty()||!person||observer==dead)return fail<Result>(source);
 auto death=query_military_actor_death(c,m,dead);if(!death.ok)return {false,{},death.error};if(!death.value||death.value->casualty_fact_id!=source)return fail<Result>(source);
 const auto* receipt=find(m.casualty_facts,source);if(!receipt||receipt->tick>c.tick()||(person->created_tick&&*person->created_tick>receipt->tick))return fail<Result>(source);
 // 同轮先后不按ID数值猜；本轮阵亡者、此前失去参战资格者不成为活见证人。
 for(const auto& f:m.casualty_facts)if(f.id==source||precedes(c,f.id,source))for(const auto& change:f.actor_changes)if(change.actor_id==observer&&has({"dead","captured","deserted"},change.status))return {true,std::nullopt,{}};
 const auto* battle_header=c.state().facts.find(receipt->battle_id);if(!battle_header||!precedes(c,receipt->battle_id,source))return fail<Result>(source);
 const MilitaryActorChange* killed=nullptr;for(const auto& change:receipt->actor_changes)if(change.actor_id==dead&&change.status=="dead"){if(killed)return fail<Result>(source);killed=&change;}if(!killed)return fail<Result>(source);
 StableId witnessed,battle_id;
 if(battle_header->domain=="military_naval"&&battle_header->kind=="naval_round"){
  if(!naval)return fail<Result>(source);auto valid=validate_naval_casualty_receipt(c,m,*naval,source,c.tick());if(!valid.ok)return {false,{},valid.error};const auto* round=find(naval->battle.rounds,receipt->battle_id);if(!round||round->casualty_source_id!=source||round->tick!=receipt->tick)return fail<Result>(source);battle_id=round->battle_id;
  for(const auto& before:round->before.units){const BattleUnit* after=nullptr;for(const auto& row:round->after.units)if(row.unit_id==before.unit_id)after=&row;const MilitaryRosterReceipt* settled=nullptr;for(const auto& row:receipt->units)if(row.unit_id==before.unit_id)settled=&row;if(!after||!settled||before.army_id!=killed->army_id)continue;
   const bool dead_here=(before.commander_actor_id==dead&&naval_commander_loss(before)!=2&&naval_commander_loss(*after)==2)||((has(before.fit,dead)||has(before.wounded,dead))&&has(after->dead,dead));if(!dead_here)continue;
   const bool observer_here=(before.commander_actor_id==observer&&naval_commander_loss(before)!=2&&naval_commander_loss(*after)!=2)||((has(before.fit,observer)||has(before.wounded,observer))&&survivor(*settled,observer));if(!observer_here)continue;
   const NavalBattleBinding* old_ship=nullptr;const NavalBattleBinding* new_ship=nullptr;for(const auto& row:round->before.bindings)if(row.unit_id==before.unit_id)old_ship=&row;for(const auto& row:round->after.bindings)if(row.unit_id==before.unit_id)new_ship=&row;if(!old_ship||!new_ship||old_ship->vessel_id!=new_ship->vessel_id||old_ship->fleet_id!=new_ship->fleet_id)return fail<Result>(source);if(!witnessed.empty())return fail<Result>(source);witnessed=before.unit_id;
  }
 }else if(battle_header->domain=="military"&&battle_header->kind=="military_battle"){
  const auto* fact=find(m.battle_facts,receipt->battle_id);const auto* battle=fact?find(m.battles,fact->session_id):nullptr;const auto* contact=battle?find(m.contact_facts,battle->source_id):nullptr;const auto* contact_header=contact?c.state().facts.find(contact->id):nullptr;
  if(!fact||!battle||!contact||fact->tick!=receipt->tick||fact->phase!="completed"||fact->contact_source_id!=contact->id||!contact->battle_id||*contact->battle_id!=battle->id||!contact_header||contact_header->kind!="military_contact"||contact_header->domain!="military"||!precedes(c,contact->id,fact->id)||!has(battle->round_receipt_ids,source))return fail<Result>(source);battle_id=battle->id;
  if(!has(battle->participant_actor_ids,observer))return {true,std::nullopt,{}};
  for(const auto& row:receipt->units){const BattleUnit* unit=nullptr;for(const auto& candidate:battle->units)if(candidate.unit_id==row.unit_id)unit=&candidate;if(!unit||unit->army_id!=killed->army_id)continue;const bool killed_here=(has(row.dead,dead)&&has(unit->member_ids,dead))||(unit->commander_actor_id==dead&&military_commander_loss(*unit)==2);if(!killed_here)continue;if(unit->commander_actor_id!=observer&&!survivor(row,observer))continue;if(!witnessed.empty())return fail<Result>(source);witnessed=row.unit_id;}
 }else return fail<Result>(source);
 if(witnessed.empty())return {true,std::nullopt,{}};
 return {true,MilitaryDeathWitness{observer,dead,source,battle_id,witnessed,receipt->tick},{}};
}
}
