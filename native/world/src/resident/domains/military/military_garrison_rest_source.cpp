// 休整只接受既有安全驻地和原足额供给；训练/动员的同半日占用不能用今天闲暇掩盖。
#include "military_garrison_rest.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> failure(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
std::optional<std::size_t> order(const CommonStore& c,const StableId& id){const auto& ids=c.state().facts.order();auto at=std::find(ids.begin(),ids.end(),id);return at==ids.end()?std::nullopt:std::optional<std::size_t>(at-ids.begin());}
}
Status validate_military_garrison_rest_policy(const MilitaryGarrisonRestPolicy& p,const MilitaryCatalog& cat){
 if(!cat.military||!cat.recovery_policy||!cat.command_shock_policy||p.profile!="supplied_garrison_rest_v1"||p.fatigue_reduction!=10||p.garrison_ids.empty()||p.garrison_ids.size()>1024)return common_failure("MilitaryGarrisonRestProfileRequired");
 std::set<StableId> ids;for(const auto& id:p.garrison_ids){if(id.empty()||!ids.insert(id).second||!has(cat.recovery_policy->garrison_ids,id))return common_failure("MilitaryGarrisonRestGarrisonRequired",id);const GarrisonDefinition* camp=nullptr;for(const auto& row:cat.garrisons)if(row.id==id)camp=&row;if(!camp||camp->ration_per_person<=0)return common_failure("MilitaryGarrisonRestGarrisonRequired",id);bool conditioned=false;for(const auto& unit:camp->units){if(!unit.initial_training&&!unit.initial_fatigue)continue;if(!unit.initial_training||!unit.initial_fatigue||*unit.initial_training<0||*unit.initial_training>100||*unit.initial_fatigue<0||*unit.initial_fatigue>100)return common_failure("MilitaryGarrisonRestConditionRequired",unit.id);conditioned=true;}if(!conditioned)return common_failure("MilitaryGarrisonRestConditionRequired",camp->id);}
 return common_success();
}
Outcome<MilitaryGarrisonRestEligibility> query_military_garrison_rest_period(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryAccess& access,const StableId& unit,const StableId& supply,const DynamicStepInput* phase,const PopulationMilitaryState* population){
 if(!cat.garrison_rest_policy)return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestProfileRequired");auto valid=validate_military_garrison_rest_policy(*cat.garrison_rest_policy,cat);if(!valid.ok)return {false,{},valid.error};
 MilitaryRecoveryPolicy safety=*cat.recovery_policy;safety.garrison_ids=cat.garrison_rest_policy->garrison_ids;
 auto source=query_military_recovery_supplied_period(c,m,cat,access,safety,unit,supply,phase,population);if(!source.ok)return {false,{},source.error};MilitaryGarrisonRestEligibility out;out.supplied=std::move(source.value);out.pause_reason=out.supplied.pause_reason;if(!out.supplied.qualified)return {true,std::move(out),{}};
 const auto boundary=order(c,supply);if(!boundary||!access.rosters_before)return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestBoundaryRequired",supply);
 auto roster=access.rosters_before({unit},out.supplied.tick,supply);if(!roster.ok)return {false,{},roster.error};if(roster.value.size()!=1||roster.value.front().id!=unit)return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestRosterRequired",unit);
 const MilitaryUnitDefinition* definition=nullptr;for(const auto& camp:cat.garrisons)for(const auto& original:camp.units)if(original.id==unit)definition=&original;
 if(!definition)return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestConditionRequired",unit);
 if(!definition->initial_training&&!definition->initial_fatigue){out.pause_reason="MilitaryGarrisonRestConditionNotEnabled";return {true,std::move(out),{}};}
 if(!definition->initial_training||!definition->initial_fatigue)return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestConditionRequired",unit);
 if(roster.value.front().fit.empty()&&roster.value.front().wounded.empty()){out.pause_reason="MilitaryGarrisonRestNoPersonnel";return {true,std::move(out),{}};}
 for(const auto& f:m.training_facts)if(f.terms.unit_id==unit){const auto at=order(c,f.id);const auto* head=c.state().facts.find(f.id);if(!at||!head||head->id!=f.id||head->kind!="military_training"||head->domain!="military"||f.start_tick<0||(f.completed_tick>=0&&f.completed_tick<f.start_tick))return failure<MilitaryGarrisonRestEligibility>("MilitaryGarrisonRestTrainingSourceRequired",f.id);
  if(*at<*boundary&&f.start_tick<out.supplied.tick&&(f.completed_tick<0||f.completed_tick>=out.supplied.tick)){out.pause_reason="MilitaryGarrisonRestTrainingActive";return {true,std::move(out),{}};}}
 out.qualified=true;out.pause_reason.clear();return {true,std::move(out),{}};
}
}
