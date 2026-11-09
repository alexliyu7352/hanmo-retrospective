// 军官暴露取同部本轮真实兵损；不会为取得死亡来源改胜负、制造兵卒或借全军均摊。
#include "military_attack_damage_core.hpp"
#include "military_commander.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(const auto& row:b.units)if(row.unit_id==id)return &row;return nullptr;}
BattleUnit* unit(MilitaryBattleRecord& b,const StableId& id){for(auto& row:b.units)if(row.unit_id==id)return &row;return nullptr;}
}
Status validate_military_commander_rule(const MilitaryCommanderLossRule& rule){if(rule.kind!="field_exposure_v1"||rule.exposure_bp<0||rule.exposure_bp>10000)return common_failure("InvalidMilitaryCommanderLossRule");return common_success();}
Status resolve_military_commander_losses(const MilitaryBattleRecord& before,MilitaryBattleRecord& after,const MilitaryCommanderLossRule& rule,const MilitaryBattleRandom& random){
 auto valid=validate_military_commander_rule(rule);if(!valid.ok)return valid;if(before.id!=after.id||before.round!=after.round||before.units.size()!=after.units.size())return common_failure("MilitaryCommanderRoundMismatch");
 std::vector<StableId> ids;std::set<StableId> people;for(const auto& row:before.units){if(row.unit_id.empty()||row.commander_actor_id.empty()||!people.insert(row.commander_actor_id).second)return common_failure("DuplicateMilitaryCommander",row.unit_id);ids.push_back(row.unit_id);}std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())return common_failure("DuplicateMilitaryUnit");
 for(const auto& id:ids){const auto* old=unit(before,id);auto* now=unit(after,id);if(!now||now->army_id!=old->army_id||now->commander_actor_id!=old->commander_actor_id||now->member_ids!=old->member_ids||military_commander_loss(*now)!=military_commander_loss(*old))return common_failure("MilitaryCommanderRoundMismatch",id);
  if(military_commander_loss(*old)==2||old->fit.empty()||old->withdrawn)continue;Integer loss=0;
  // 真正编入兵员的军官沿原名册唯一损失，绝不再抽第二次或重复占一人。
  if(has(old->member_ids,old->commander_actor_id)){if(!has(old->dead,old->commander_actor_id)&&has(now->dead,old->commander_actor_id))loss=2;else if(has(old->fit,old->commander_actor_id)&&has(now->wounded,old->commander_actor_id))loss=1;}
  else {Integer killed=0,hurt=0;for(const auto& person:old->fit){if(has(now->dead,person))++killed;else if(has(now->wounded,person))++hurt;}if(killed+hurt==0||rule.exposure_bp==0)continue;if(!random)return common_failure("MilitaryRandomServiceRequired");auto draw=random(after.rng_seed,after.rng_state,1,10000);if(!draw.ok)return {false,{},draw.error};if(draw.value<1||draw.value>10000)return common_failure("InvalidMilitaryRandomDraw");loss=original_military_commander_exposure_loss(killed,hurt,Integer(old->fit.size()),rule.exposure_bp,draw.value);}
  if(loss>military_commander_loss(*old)){mark_military_commander_loss(*now,loss);MilitaryBattleEvent event;event.kind="commander_loss";event.unit_id=id;event.code=loss==2?"MilitaryCommanderKilled":"MilitaryCommanderWounded";after.last_events.push_back(std::move(event));}
 }
 return common_success();
}
}
