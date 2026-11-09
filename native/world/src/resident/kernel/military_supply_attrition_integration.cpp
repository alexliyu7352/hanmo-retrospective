// 薄桥只借当前候选与严格phase，任一军籍/人口/经济后写失败由原World事务整体恢复。
#include "military_supply_attrition_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/population_military_attrition.hpp"
#include <limits>
#include <algorithm>
#include "resident/domains/military/military_supply_attrition_profile.hpp"
namespace hanmo::resident {
Status step_military_supply_attrition_world(WorldBundle& w,const DynamicStepInput& phase){
 if(!w.military_supply_attrition_enabled())return common_failure("MilitarySupplyAttritionProfileRequired");
 if(phase.from_tick<0||phase.from_tick!=w.common.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return common_failure("MilitarySupplyAttritionPhaseClockMismatch");
 auto access=w.services.military_access(phase.to_tick,&phase);
 access.rosters_before=[&](const std::vector<StableId>& units,Integer tick,const StableId& fact){return military_rosters_before(w.common,w.military,*w.catalog->military,units,tick,fact,phase,w.population_military_enabled()?&w.population_military:nullptr,w.population_military_enabled()?w.catalog->population_military.get():nullptr);};
 auto lost=step_military_supply_attrition(w.common,w.military,*w.catalog->military,access,phase);if(!lost.ok)return {false,{},lost.error};
 if(w.population_military_enabled()){auto context=w.population_write(phase.to_tick);auto population=reconcile_population_military_attrition(context,phase.to_tick);if(!population.ok)return {false,{},population.error};}
 return common_success();
}
Status validate_military_supply_attrition_world(const WorldBundle& w){
 const bool marked=std::any_of(w.catalog->identity.module_versions.begin(),w.catalog->identity.module_versions.end(),[](const auto& m){return m.first==military_supply_attrition_module;});if(marked!=w.military_supply_attrition_enabled())return common_failure("MilitarySupplyAttritionProfileMismatch");
 if(!marked)return common_success();
 auto access=w.services.military_access();return validate_military_supply_attrition_history(w.common,w.military,*w.catalog->military,access);
}
}
