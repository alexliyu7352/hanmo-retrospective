// 真实有效兵力与当轮筑城级决定原始攻门值；纯函数不扣材料、不改变城防、不消费随机。
#include "military_siege_engineering.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// JSON实字段只能是明确集合，禁止通过省略或重复新段权源绕过后续冷验证。
bool exact_fields(FieldOrder actual,FieldOrder expected){if(actual.empty())return true;std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}
Integer level_at(Integer xp){return xp>=1200?4:xp>=650?3:xp>=300?2:xp>=100?1:0;}
}
Status validate_military_siege_engineering_rule(const MilitarySiegeEngineeringRule& rule){return rule.profile=="commander_fortification_05_v1"?common_success():common_failure("MilitarySiegeEngineeringProfileMismatch");}
Outcome<Integer> military_siege_engineering_assault_base(const MilitarySiegeEngineeringRule& rule,Integer soldiers,Integer level,bool engine){auto valid=validate_military_siege_engineering_rule(rule);if(!valid.ok)return {false,{},valid.error};if(soldiers<1||soldiers>500||level<0||level>4)return {false,{},common_failure("MilitarySiegeEngineeringQualificationOutOfRange").error};return {true,20+5*level+soldiers/50+(engine?20:0),{}};}
Status validate_military_siege_engineering_preparation_shape(const MilitarySiegeEngineeringPreparation& p){
 if(p.id.empty()||p.siege_id.empty()||p.topology_id.empty()||p.from_tick<0||p.from_tick>=2147483647||p.to_tick!=p.from_tick+1||p.round<1||p.round>p.from_tick+1||p.units.size()>4096||!exact_fields(p.field_order,{"id","siege_id","topology_id","previous_preparation_id","from_tick","to_tick","round","units"}))return common_failure("MilitarySiegeEngineeringPreparationShape",p.id);
 StableId previous;for(const auto& u:p.units){const auto kind=static_cast<int>(u.command_source.kind);
  if(u.unit_id.empty()||(!previous.empty()&&u.unit_id<=previous)||u.army_id.empty()||u.commander_actor_id.empty()||u.command_source.id.empty()||kind<0||kind>3||(kind==0&&u.command_source.id!=u.unit_id)||u.fit_count<0||u.fit_count>500||u.experience<0||u.experience>1200||u.level!=level_at(u.experience)||(u.authored_actor_id.empty()&&u.causal_fact_ids.empty()))return common_failure("MilitarySiegeEngineeringUnitShape",u.unit_id);
  previous=u.unit_id;std::set<StableId> seen;for(const auto& id:u.causal_fact_ids)if(id.empty()||!seen.insert(id).second)return common_failure("MilitarySiegeEngineeringCausalSourceMismatch",u.unit_id);
  if(!exact_fields(u.field_order,{"unit_id","army_id","commander_actor_id","authored_actor_id","command_source","fit_count","experience","level","causal_fact_ids"})||!exact_fields(u.source_field_order,{"kind","id"}))return common_failure("MilitarySiegeEngineeringUnitShape",u.unit_id);
 }
 return common_success();
}
}
