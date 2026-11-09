#include "resident/domains/military/military_model.hpp"
// 逐原轮次只读核对独立军官损失；不以目前阵亡状态倒填因果，也不冒原陆战的完整随机重演。
#include "military_commander_history.hpp"
#include "military_commander.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(const auto& row:b.units)if(row.unit_id==id)return &row;return nullptr;}
const MilitaryRosterReceipt* roster(const MilitaryCasualtyFact& f,const StableId& id){for(const auto& row:f.units)if(row.unit_id==id)return &row;return nullptr;}
Status fail(const StableId& id){return common_failure("MilitaryCommanderLossSourceMismatch",id);}
}
Outcome<std::vector<MilitaryActorChange>> military_commander_actor_changes(const CommonRead& c,const MilitaryState& s,const MilitaryBattleRecord& before,const MilitaryBattleRecord& after){
 std::vector<MilitaryActorChange> out;if(before.id!=after.id)return {false,{},fail(after.id).error};
 for(const auto& next:after.units){const auto* old=unit(before,next.unit_id);if(!old||old->army_id!=next.army_id||old->commander_actor_id!=next.commander_actor_id)return {false,{},fail(next.unit_id).error};auto prior=military_commander_loss(*old),loss=military_commander_loss(next);if(loss<prior||loss>2)return {false,{},fail(next.unit_id).error};if(loss==prior||has(old->member_ids,old->commander_actor_id))continue;const auto* actor=c.actor(old->commander_actor_id);const auto* army=find(s.armies,old->army_id);if(!actor||!actor->alive||!actor->available||!army||!has(army->member_actor_ids,actor->id))return {false,{},fail(next.unit_id).error};MilitaryActorChange change;change.actor_id=actor->id;change.army_id=army->id;change.status=loss==2?"dead":"wounded";out.push_back(std::move(change));}
 return {true,std::move(out),{}};
}
Status validate_military_commander_history(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const MilitaryBattleRecord& battle){
 const auto* board=find(cat.battle_catalog.battlefields,battle.battlefield_id);if(!board)return fail(battle.id);std::map<StableId,Integer> losses;std::vector<StableId> ids;for(const auto& u:battle.units)ids.push_back(u.unit_id);
 for(const auto& rid:battle.round_receipt_ids){const auto* receipt=find(s.casualty_facts,rid);if(!receipt)return fail(rid);std::set<StableId> seen;
  std::optional<std::vector<MilitaryUnitRecord>> prior;if(board->commander_losses){if(!access.rosters_before)return common_failure("MilitaryCommanderHistoryReadRequired",rid);auto read=access.rosters_before(ids,receipt->tick,rid);if(!read.ok)return {false,{},read.error};prior=std::move(read.value);}
  for(const auto& change:receipt->actor_changes){const BattleUnit* captain=nullptr;for(const auto& u:battle.units)if(u.commander_actor_id==change.actor_id){captain=&u;break;}if(!captain)continue;if(!seen.insert(change.actor_id).second||change.army_id!=captain->army_id)return fail(rid);
   const bool enlisted=has(captain->member_ids,change.actor_id);if(!board->commander_losses){if(!enlisted)return fail(rid);continue;}if(change.status!="wounded"&&change.status!="dead"){if(enlisted)continue;return fail(rid);}const Integer loss=change.status=="dead"?2:1;if(loss<=losses[captain->unit_id])return fail(rid);const auto* old=find(*prior,captain->unit_id);const auto* row=roster(*receipt,captain->unit_id);if(!old||!row||old->army_id!=captain->army_id||old->commander_actor_id!=captain->commander_actor_id||old->member_ids!=row->member_ids)return fail(rid);
   if(enlisted){if(!has(loss==2?row->dead:row->wounded,change.actor_id))return fail(rid);}else{Integer killed=0,hurt=0;for(const auto& person:old->fit){if(has(row->dead,person))++killed;else if(has(row->wounded,person))++hurt;}if(board->commander_losses->exposure_bp<=0||old->fit.empty()||(loss==2?killed:killed+hurt)==0)return fail(rid);}
   losses[captain->unit_id]=loss;if(loss==2){const auto* person=c.actor(change.actor_id);if(!person||person->alive||person->available||person->hp!=0||!person->action_id.empty())return fail(rid);Integer count=0;for(const auto& fact:s.casualty_facts)for(const auto& dead:fact.actor_changes)if(dead.actor_id==change.actor_id&&dead.status=="dead")++count;if(count!=1)return fail(rid);}
  }
 }
 for(const auto& u:battle.units)if(military_commander_loss(u)!=losses[u.unit_id])return fail(u.unit_id);return common_success();
}
}
