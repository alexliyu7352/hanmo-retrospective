// 同速轮转避免固定ID永远占先；每部仅一项，已退出、失帅、无兵者不获新行动。
#include "military_sequential_rounds.hpp"
#include "military_commanded_damage.hpp"
#include "military_commander.hpp"
#include "military_rout_status.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
Status validate_military_sequential_rule(const MilitarySequentialRoundRule& r){return r.profile=="entry_initiative_05_v1"?common_success():common_failure("MilitarySequentialProfileMismatch");}
bool military_sequential_round_in_use(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c){return c.sequential_round_rule.has_value()&&b.rout_retreat_enabled;}
Outcome<std::vector<MilitarySequentialTurn>> query_military_sequential_turns(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c){
 using R=std::vector<MilitarySequentialTurn>;auto fail=[](const StableId& code,const StableId& path={}) -> Outcome<R>{return {false,{},common_failure(code,path).error};};
 if(!military_sequential_round_in_use(b,c)||!c.commanded_damage_rule||!b.commanded_damage)return fail("MilitarySequentialPrerequisiteRequired",b.id);
 auto checked=validate_military_sequential_rule(*c.sequential_round_rule);if(!checked.ok)return {false,{},checked.error};checked=validate_military_commanded_damage_shape(b,c,*b.commanded_damage);if(!checked.ok)return {false,{},checked.error};
 if(b.phase!="planning"||b.round<1||b.round>9999)return fail("MilitarySequentialRoundRequired",b.id);
 std::vector<StableId> ordered;for(const auto& u:b.units)ordered.push_back(u.unit_id);std::sort(ordered.begin(),ordered.end());if(ordered.empty()||std::adjacent_find(ordered.begin(),ordered.end())!=ordered.end())return fail("DuplicateMilitaryUnit",b.id);
 R result;
 for(const auto& u:b.units){
  if(u.withdrawn||u.fit.empty()||(military_commander_loss(u)==2&&!military_unit_routing(u)))continue;
  const MilitaryCommandedDamageUnit* source=nullptr;for(const auto& r:b.commanded_damage->units)if(r.unit_id==u.unit_id)source=&r;
  const TroopDefinition* troop=nullptr;for(const auto& t:c.troop_types)if(t.id==u.troop_type_id)troop=&t;
  if(!source||!troop||source->commander_actor_id!=u.commander_actor_id)return fail("MilitarySequentialUnitSourceMismatch",u.unit_id);
  Integer speed=troop->category=="infantry"?1:troop->category=="archer"?0:troop->category=="cavalry"?3:-1;if(speed<0)return fail("MilitaryCommandedTroopUnsupported",u.unit_id);
  result.push_back({u.unit_id,u.commander_actor_id,source->leadership/10+speed,0});
 }
 // 同值组独立轮转，异速单位插入或退出不得改变本组轮换周期。
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.initiative!=b.initiative?a.initiative>b.initiative:a.unit_id<b.unit_id;});
 for(std::size_t first=0;first<result.size();){std::size_t last=first+1;while(last<result.size()&&result[last].initiative==result[first].initiative)++last;std::rotate(result.begin()+first,result.begin()+first+b.round%(last-first),result.begin()+last);for(std::size_t at=first;at<last;++at)result[at].rotation_rank=Integer(at-first);first=last;}
 return {true,std::move(result),{}};
}
}
