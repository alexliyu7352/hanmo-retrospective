#include "military_attack_damage_core.hpp"
#include "dynamic_land_commander.hpp"
#include "military_commander.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {using namespace movement_detail;
Outcome<DynamicLandCommanderMerge> merge_dynamic_land_commander_exposure(const MilitaryBattleCatalog& catalog,const StableId& battlefield,const std::vector<DynamicLandPostureUnit>& before,const DynamicLandMergedAllocation& allocated,const SourceRef& group,const DynamicMilitarySessionRandom& rng){return guard<DynamicLandCommanderMerge>([&]{
 if(group.domain!="military"||group.kind!="dynamic_effect_group"||group.id.empty())fail("DynamicMilitaryEffectGroupRequired",group.id);
 const BattlefieldDefinition* board=nullptr;for(const auto& row:catalog.battlefields)if(row.id==battlefield){if(board)fail("DuplicateMilitaryDefinition",battlefield);board=&row;}if(!board)fail("UnknownMilitaryDefinition",battlefield);
 DynamicLandCommanderMerge out;out.rng_after=rng;take(validate_dynamic_military_random_state(rng));if(!board->commander_losses){for(const auto& u:before)if(military_commander_loss(u.original_unit)!=0)fail("MilitaryCommanderRuleRequired",u.original_unit.unit_id);return out;}const auto& rule=*board->commander_losses;take(validate_military_commander_rule(rule));
 std::vector<const DynamicLandPostureUnit*> ordered;std::set<StableId> people,units;for(const auto& u:before){if(u.original_unit.unit_id.empty()||u.original_unit.commander_actor_id.empty()||!units.insert(u.original_unit.unit_id).second||!people.insert(u.original_unit.commander_actor_id).second)fail("DuplicateMilitaryCommander",u.original_unit.unit_id);ordered.push_back(&u);}std::sort(ordered.begin(),ordered.end(),[](auto* a,auto* z){return a->original_unit.unit_id<z->original_unit.unit_id;});
 for(const auto* input:ordered){const auto& u=input->original_unit;const auto old=military_commander_loss(u);if(std::count_if(u.statuses.begin(),u.statuses.end(),[](const auto& x){return x.first=="military_commander_loss_v1";})>1)fail("DuplicateMilitaryCommanderLossSource",u.unit_id);if(old<0||old>2||u.fit.size()>4096)fail("InvalidMilitaryCommanderLoss",u.unit_id);if(old==2||u.fit.empty()||u.withdrawn)continue;
  const bool enlisted=std::find(u.member_ids.begin(),u.member_ids.end(),u.commander_actor_id)!=u.member_ids.end();Integer loss=0,killed=0,hurt=0;std::set<StableId> selected;
  for(const auto& a:allocated.member_allocations)if(a.unit_ref==u.unit_id){if(!selected.insert(a.member_id).second||std::find(u.fit.begin(),u.fit.end(),a.member_id)==u.fit.end()||(a.resulting_status!="dead"&&a.resulting_status!="wounded"))fail("DynamicMilitaryCommanderAllocationMismatch",u.unit_id);if(a.resulting_status=="dead")++killed;else ++hurt;if(enlisted&&a.member_id==u.commander_actor_id)loss=a.resulting_status=="dead"?2:1;}
  // An enlisted commander consumes the real E allocation once, without another
  // exposure draw. Otherwise only this unit's actual complete losses are used.
  if(!enlisted&&killed+hurt>0&&rule.exposure_bp>0){auto slot=take(draw_dynamic_military_random_in_candidate(out.rng_after,group,DynamicMilitaryRandomStage::commander_exposure,1,10000));loss=original_military_commander_exposure_loss(killed,hurt,Integer(u.fit.size()),rule.exposure_bp,slot.draw_result);out.rng_slots.push_back(std::move(slot));}
  if(loss>old)out.consequences.push_back({u.unit_id,u.army_id,u.commander_actor_id,old,loss,enlisted,group});
 }
 take(validate_dynamic_military_random_chain(rng,out.rng_slots,out.rng_after));return out;
});}
}
