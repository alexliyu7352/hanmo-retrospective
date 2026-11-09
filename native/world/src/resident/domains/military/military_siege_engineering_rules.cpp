// 纯绑定不查询今日技能、不生成来源；真实World在外层先验证原XP与军事权源。
#include "military_siege_engineering_rules.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
Status bind_military_siege_engineering_observation(const SiegeRecord& siege,const MilitarySiegeEngineeringPreparation& p,SiegeObservation& o){
 auto checked=validate_military_siege_engineering_preparation_shape(p);if(!checked.ok)return checked;
 if(p.siege_id!=siege.id||p.topology_id!=siege.topology_id||p.from_tick!=siege.last_tick||p.from_tick!=o.tick||p.round!=siege.round||siege.phase!="active")return common_failure("MilitarySiegeEngineeringObservationMismatch",siege.id);
 std::vector<StableId> expected;for(const auto& node:siege.unit_nodes)expected.push_back(node.first);std::sort(expected.begin(),expected.end());
 if(expected.size()!=p.units.size()||std::adjacent_find(expected.begin(),expected.end())!=expected.end())return common_failure("MilitarySiegeEngineeringRosterMismatch",siege.id);
 for(std::size_t i=0;i<expected.size();++i){const auto& proof=p.units[i];const auto found=std::find_if(o.units.begin(),o.units.end(),[&](const auto& row){return row.id==expected[i];});
  if(proof.unit_id!=expected[i]||found==o.units.end()||found->army_id!=proof.army_id||found->fit_count!=proof.fit_count)return common_failure("MilitarySiegeEngineeringRosterMismatch",expected[i]);
 }
 o.engineering_preparation=p;return common_success();
}
}
