// 双方倒地只改变显式选中目标的终局优先级；原人物后果/费用/事实一次性出口继续归原会话。
#include "personal_simultaneous_outcomes.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
bool personal_simultaneous_outcome_selected(const PersonalCatalog& c,const StableId& id){return std::find(c.simultaneous_outcome_objectives.begin(),c.simultaneous_outcome_objectives.end(),id)!=c.simultaneous_outcome_objectives.end();}
Status validate_personal_simultaneous_outcomes(const PersonalCatalog& c){
 if(c.simultaneous_outcome_objectives.empty())return common_success();
 if(!c.enabled||c.simultaneous_outcome_objectives.size()>32)return common_failure("PersonalSimultaneousOutcomeProfileMismatch");
 std::set<StableId> seen;
 for(const auto& id:c.simultaneous_outcome_objectives){auto o=c.objectives.find(id);if(id.empty()||!seen.insert(id).second||!o||(o->kind!="sparring"&&o->kind!="skirmish")||o->escort_model)return common_failure("PersonalSimultaneousOutcomeObjectiveRequired",id);}
 return common_success();
}
Status validate_personal_simultaneous_outcome_modules(const std::vector<std::pair<StableId,StableId>>& modules,bool enabled){
 Integer count=0;bool dynamic=false;for(const auto& m:modules){if(m.first==personal_simultaneous_outcomes_module){if(m.second!="1"||++count>1)return common_failure("PersonalSimultaneousOutcomeProfileMismatch");}if(m.first=="dynamic_world"&&m.second=="local_society_v1")dynamic=true;}
 return (count==1)==enabled&&(!enabled||dynamic)?common_success():common_failure("PersonalSimultaneousOutcomeProfileMismatch");
}
Status configure_personal_simultaneous_outcomes(const BoundaryValue& data,const BoundaryValue& scenario,PersonalCatalog& c){
 try{
  const auto* policy=scenario.find(personal_simultaneous_outcomes_module);if(!policy){c.simultaneous_outcome_objectives.clear();return common_success();}
  boundary::fields(*policy,{"version","objective_ids"},{},personal_simultaneous_outcomes_module);
  const auto& rows=boundary::object(*policy,personal_simultaneous_outcomes_module);std::set<StableId> keys;for(const auto& row:rows)if(!keys.insert(row.first).second)return common_failure("PersonalSimultaneousOutcomeDuplicateField");
  if(boundary::get_int(*policy,"version",personal_simultaneous_outcomes_module)!=1)return common_failure("PersonalSimultaneousOutcomeVersionUnsupported");
  const auto* dynamic=scenario.find("dynamic_world");if(!dynamic||boundary::get_string(*dynamic,"profile","dynamic_world")!="local_society_v1")return common_failure("PersonalSimultaneousOutcomeNativeProfileRequired");
  bool capability=false;if(const auto* caps=data.find("required_capabilities"))for(const auto& v:boundary::array(*caps,"required_capabilities"))if(boundary::string(v,"required_capabilities")==personal_simultaneous_outcomes_capability)capability=true;
  if(!capability||!c.enabled)return common_failure("PersonalSimultaneousOutcomeCapabilityRequired");
  auto selected=c;selected.simultaneous_outcome_objectives.clear();
  for(const auto& v:boundary::array(boundary::field(*policy,"objective_ids",personal_simultaneous_outcomes_module),"objective_ids"))selected.simultaneous_outcome_objectives.push_back(boundary::string(v,"objective_ids"));
  if(selected.simultaneous_outcome_objectives.empty())return common_failure("PersonalSimultaneousOutcomeObjectiveRequired");
  const auto& enabled=boundary::field(scenario,"enabled","scenario");const auto enabled_ids=boundary::strings(boundary::field(enabled,"personal_objectives","enabled"),"enabled/personal_objectives");for(const auto& id:selected.simultaneous_outcome_objectives)if(std::find(enabled_ids.begin(),enabled_ids.end(),id)==enabled_ids.end())return common_failure("PersonalSimultaneousOutcomeNotEnabled",id);
  auto valid=validate_personal_simultaneous_outcomes(selected);if(!valid.ok)return valid;
  c.simultaneous_outcome_objectives=std::move(selected.simultaneous_outcome_objectives);return common_success();
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
bool personal_mutual_defeat_draw(const PersonalBattle& b,const PersonalCatalog& c){
 if(!personal_simultaneous_outcome_selected(c,b.objective_id))return false;
 auto controller=b.units.find(b.controller_id);if(!controller||controller->hp!=0||controller->withdrawn_round)return false;
 bool opponent=false;
 for(const auto& row:b.units.rows()){
  const auto& u=row.second;
  if(u.side==controller->side){if(u.protected_actor&&u.hp<=0)return false;continue;}
  opponent=true;if(u.hp!=0||u.withdrawn_round)return false;
 }
 return opponent;
}
}
