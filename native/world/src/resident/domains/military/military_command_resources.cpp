// 初始40由真实入场创建器应用；此处只按冻结兵法恢复5+级且总量封顶100，未启用世界不得调用为授权。
#include "military_command_resources.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Status validate_military_command_resources_rule(const MilitaryCommandResourcesRule& r){return r.profile=="entry_tactics_recovery_05_v1"?common_success():common_failure("MilitaryCommandResourcesProfileMismatch");}
namespace {
bool fields(FieldOrder actual,FieldOrder expected){if(actual.empty())return true;std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}
Integer level(Integer xp){return xp>=1200?4:xp>=650?3:xp>=300?2:xp>=100?1:0;}
}
Status validate_military_command_resources_shape(const MilitaryBattleRecord& b,const MilitaryCommandResourcesState& state){
 if(!b.rout_retreat_enabled||!b.commanded_damage||state.entry_tick<0||state.entry_tick!=b.created_tick||state.units.empty()||state.units.size()!=b.units.size()||!fields(state.field_order,{"entry_tick","units"}))return common_failure("MilitaryCommandResourcesShapeMismatch",b.id);
 StableId previous;std::set<StableId> actors;for(const auto& row:state.units){const BattleUnit* u=nullptr;for(const auto& candidate:b.units)if(candidate.unit_id==row.unit_id){if(u)return common_failure("MilitaryCommandResourcesUnitMismatch",row.unit_id);u=&candidate;}
  if(!u||row.unit_id.empty()||(!previous.empty()&&row.unit_id<=previous)||row.commander_actor_id.empty()||row.commander_actor_id!=u->commander_actor_id||!actors.insert(row.commander_actor_id).second)return common_failure("MilitaryCommandResourcesUnitMismatch",row.unit_id);previous=row.unit_id;
  if(row.experience<0||row.experience>1200||row.level!=level(row.experience)||(row.authored_actor_id.empty()&&row.causal_fact_ids.empty())||!fields(row.field_order,{"unit_id","commander_actor_id","authored_actor_id","experience","level","causal_fact_ids"}))return common_failure("MilitaryCommandResourcesQualificationMismatch",row.unit_id);
  std::set<StableId> causes;for(const auto& id:row.causal_fact_ids)if(id.empty()||!causes.insert(id).second)return common_failure("MilitaryCommandResourcesCausalSourceMismatch",row.unit_id);
 }
 return common_success();
}
Outcome<MilitaryCommandResourceQuote> quote_military_command_resource_recovery(Integer before,Integer tactics_level){
 if(before<0||before>100||tactics_level<0||tactics_level>4)return {false,{},common_failure("MilitaryCommandResourcesOutOfRange").error};const auto available=std::min<Integer>(100,before+5+tactics_level);return {true,{before,available-before,available},{}};
}
Outcome<MilitaryCommandResourceQuote> query_military_command_resources(const MilitaryBattleRecord& b,const MilitaryCommandResourcesState& state,const StableId& id){
 auto checked=validate_military_command_resources_shape(b,state);if(!checked.ok)return {false,{},checked.error};const BattleUnit* u=nullptr;const MilitaryCommandResourcesUnit* source=nullptr;for(const auto& row:b.units)if(row.unit_id==id)u=&row;for(const auto& row:state.units)if(row.unit_id==id)source=&row;if(!u||!source)return {false,{},common_failure("MilitaryCommandResourcesUnitMismatch",id).error};return quote_military_command_resource_recovery(u->tactic_points,source->level);
}
}
