// 军籍、人口与经济cohort在同一原候选推进；任何后写失败交上层完整World事务恢复。
#include "military_recovery_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_garrison_rest.hpp"
#include "resident/domains/military/military_garrison_rest_profile.hpp"
#include <algorithm>
#include "growth_recovery_integration.hpp"
#include "resident/domains/military/military_recovery_growth.hpp"
#include "resident/domains/military/military_recovery_rules.hpp"
#include "resident/domains/military/population_military_recovery.hpp"
#include <limits>
namespace hanmo::resident {
Status step_military_garrison_rest_world(WorldBundle& w,const DynamicStepInput& phase){
 if(!w.military_garrison_rest_enabled())return common_failure("MilitaryGarrisonRestProfileRequired");
 auto access=w.services.military_access(phase.to_tick,&phase);
 access.rosters_before=[&](const std::vector<StableId>& units,Integer tick,const StableId& fact){return military_rosters_before(w.common,w.military,*w.catalog->military,units,tick,fact,phase,w.population_military_enabled()?&w.population_military:nullptr,w.population_military_enabled()?w.catalog->population_military.get():nullptr);};
 auto rested=step_military_garrison_rest(w.common,w.military,*w.catalog->military,access,phase,w.population_military_enabled()?&w.population_military:nullptr);return rested.ok?common_success():Status{false,{},rested.error};
}
Status validate_military_garrison_rest_world(const WorldBundle& w){
 const bool marked=std::any_of(w.catalog->identity.module_versions.begin(),w.catalog->identity.module_versions.end(),[](const auto& m){return m.first==military_garrison_rest_module;});if(marked!=w.military_garrison_rest_enabled())return common_failure("MilitaryGarrisonRestProfileMismatch");
 if(!w.catalog->military)return w.military.garrison_rest.facts.empty()&&w.military.garrison_rest.field_order.empty()?common_success():common_failure("UnexpectedMilitaryGarrisonRest");auto access=w.services.military_access();return validate_military_garrison_rest_history(w.common,w.military,*w.catalog->military,access,nullptr,w.population_military_enabled()?&w.population_military:nullptr);
}
Status step_military_wounded_recovery_world(WorldBundle& w,const DynamicStepInput& phase){
 if(!w.military_recovery_enabled())return common_failure("MilitaryRecoveryProfileRequired");
 if(phase.from_tick<0||phase.from_tick!=w.common.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return common_failure("MilitaryRecoveryPhaseClockMismatch");
 auto source=make_growth_recovered_actor_read(w,&phase);if(!source)return common_failure("MilitaryRecoveryPersonalProfileRequired");MilitaryGrowthRecoveryRead bodies(*source);
 auto access=w.services.military_access(phase.to_tick,&phase);
 // phase已捕获才建立此借用；不在preflight以前复制尚未初始化的DynamicStepInput。
 access.rosters_before=[&](const std::vector<StableId>& units,Integer tick,const StableId& fact){return military_rosters_before(w.common,w.military,*w.catalog->military,units,tick,fact,phase,w.population_military_enabled()?&w.population_military:nullptr,w.population_military_enabled()?w.catalog->population_military.get():nullptr);};
 auto recovered=step_military_wounded_recovery(w.common,w.military,*w.catalog->military,access,w.military.recovery,*w.catalog->military->recovery_policy,&bodies,phase,w.population_military_enabled()?&w.population_military:nullptr);if(!recovered.ok)return {false,{},recovered.error};
 if(w.population_military_enabled()){auto context=w.population_write(phase.to_tick);auto population=reconcile_population_military_recovery(context,phase.to_tick);if(!population.ok)return {false,{},population.error};}
 return common_success();
}
Status validate_military_wounded_recovery_world_sources(const WorldBundle& w){
 if(!w.military_recovery_enabled())return common_failure("MilitaryRecoveryProfileRequired");auto source=make_growth_recovered_actor_read(w);if(!source)return common_failure("MilitaryRecoveryPersonalProfileRequired");MilitaryGrowthRecoveryRead bodies(*source);auto access=w.services.military_access();
 return validate_military_recovery_actor_sources(w.common,w.military,*w.catalog->military,access,w.military.recovery,*w.catalog->military->recovery_policy,&bodies,w.population_military_enabled()?&w.population_military:nullptr);
}
}
