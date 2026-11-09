#include "dynamic_land_disposition.hpp"
#include "military_commander.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
bool operational(const BattleUnit& u){return !u.withdrawn&&!u.fit.empty()&&u.morale>0&&military_commander_loss(u)!=2;}
struct ActualUnit {BattleUnit row;MilitarySpatialPoint point;};
ActualUnit& unit(std::vector<ActualUnit>& rows,const StableId& id){for(auto& u:rows)if(u.row.unit_id==id)return u;fail("DynamicMilitaryDispositionUnitRequired",id);}
bool at_exit(const BattlefieldDefinition& board,const ActualUnit& u,ExactArithmeticBudget b){for(const auto& exit:board.exits)if(exit.side==u.row.side){const MilitarySpatialPoint p{take(spatial_integer(exit.position.q,b)),take(spatial_integer(exit.position.r,b))};if(take(military_spatial_points_equal(p,u.point,b)))return true;}return false;}
}
Outcome<DynamicLandDispositionMerge> merge_dynamic_land_rout_and_withdrawal(const MilitaryBattleCatalog& catalog,const StableId& battlefield,const std::vector<DynamicLandPostureUnit>& snapshot,const DynamicLandMergedAllocation& allocation,const DynamicLandCommanderMerge& commanders,const DynamicLandCommandShockMerge& shock,const std::vector<DynamicActualWithdrawalRead>& withdrawals,const SourceRef& group,ExactArithmeticBudget b){return guard<DynamicLandDispositionMerge>([&]{
 if(group.domain!="military"||group.kind!="dynamic_effect_group"||group.id.empty())fail("DynamicMilitaryEffectGroupRequired",group.id);
 const BattlefieldDefinition* board=nullptr;for(const auto& row:catalog.battlefields)if(row.id==battlefield){if(board)fail("DuplicateMilitaryDefinition",battlefield);board=&row;}if(!board)fail("UnknownMilitaryDefinition",battlefield);
 std::vector<ActualUnit> rows;std::set<StableId> identities,people;for(const auto& original:snapshot){const auto& u=original.original_unit;if(u.unit_id.empty()||u.army_id.empty()||u.side<0||u.side>1||u.morale<0||u.morale>100||!identities.insert(u.unit_id).second)fail("DynamicMilitaryDispositionRosterMismatch",u.unit_id);std::set<StableId> partition,members(u.member_ids.begin(),u.member_ids.end());for(const auto* pool:{&u.fit,&u.wounded,&u.dead,&u.captured,&u.deserted})for(const auto& id:*pool)if(id.empty()||!partition.insert(id).second||!people.insert(id).second)fail("DynamicMilitaryDispositionRosterMismatch",id);if(members.size()!=u.member_ids.size()||members!=partition)fail("DynamicMilitaryDispositionRosterMismatch",u.unit_id);rows.push_back({u,original.actual_position});}
 std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& z){return a.row.unit_id<z.row.unit_id;});
 for(const auto& selected:allocation.member_allocations){auto& u=unit(rows,selected.unit_ref).row;const auto found=std::find(u.fit.begin(),u.fit.end(),selected.member_id);if(found==u.fit.end()||(selected.resulting_status!="wounded"&&selected.resulting_status!="dead"))fail("DynamicMilitaryDispositionAllocationMismatch",selected.member_id);u.fit.erase(found);(selected.resulting_status=="dead"?u.dead:u.wounded).push_back(selected.member_id);}
 auto morale=[&](const auto& changes){for(const auto& change:changes){auto& u=unit(rows,change.unit_ref).row;if(u.morale!=change.before_morale||change.after_morale<0||change.after_morale>100||change.delta!=change.after_morale-change.before_morale)fail("DynamicMilitaryDispositionMoraleMismatch",u.unit_id);u.morale=change.after_morale;}};morale(allocation.morale_changes);morale(shock.morale_changes);
 for(const auto& change:commanders.consequences){auto& u=unit(rows,change.unit_ref).row;if(u.commander_actor_id!=change.actor_ref||u.army_id!=change.army_ref||!same_source(change.actual_effect_group,group))fail("DynamicMilitaryDispositionCommanderMismatch",change.actor_ref);mark_military_commander_loss(u,change.after_loss);}
 DynamicLandDispositionMerge out;std::set<StableId> routed;for(const auto& u:rows)if(!u.row.withdrawn&&!u.row.fit.empty()&&u.row.morale==0)routed.insert(u.row.unit_id);
 if(!routed.empty()&&catalog.rout_retreat_enabled&&board->rout_retreat)fail("DynamicMilitaryActualRoutRetreatProgressRequired",group.id);
 // This is the unchanged original immediate-rout leaf. Exact axial distance
 // replaces only the original position input; no rounded cell is substituted.
 for(auto& u:rows)if(routed.count(u.row.unit_id)){Integer enemies=0;const ActualUnit* captor=nullptr;for(const auto& other:rows)if(!routed.count(other.row.unit_id)&&operational(other.row)&&other.row.side!=u.row.side){const auto distance=take(military_axial_length(u.point,other.point,b));if(compare(distance,{},b)<=0)fail("DynamicMilitaryDistinctUnitsOverlap",u.row.unit_id);if(compare(distance,integer(1,b),b)<=0){++enemies;if(!captor)captor=&other;}}
  auto fit=u.row.fit;std::sort(fit.begin(),fit.end());const auto count=enemies>=2?fit.size():enemies==1?fit.size()/2:0;
  for(std::size_t n=0;n<fit.size();++n){const bool captured=n<count;out.member_dispositions.push_back({u.row.unit_id,fit[n],"fit",captured?"captured":"deserted",captured?captor->row.unit_id:StableId{},captured?captor->row.army_id:StableId{},group});}
  if(enemies>=2){auto wounded=u.row.wounded;std::sort(wounded.begin(),wounded.end());for(const auto& id:wounded)out.member_dispositions.push_back({u.row.unit_id,id,"wounded","captured",captor->row.unit_id,captor->row.army_id,group});}u.row.fit.clear();if(enemies>=2)u.row.wounded.clear();
 }
 std::set<StableId> seen;for(const auto& request:withdrawals){if(!seen.insert(request.unit_ref).second||!source_valid(request.accepted_withdrawal_source)||!source_valid(request.actual_exit_geometry_source))fail("DynamicMilitaryActualWithdrawalRequired",request.unit_ref);auto& u=unit(rows,request.unit_ref);if(!at_exit(*board,u,b))fail("DynamicMilitaryActualExitGeometryMismatch",request.unit_ref);if(!operational(u.row))continue;out.actual_exits.push_back({request.unit_ref,request.accepted_withdrawal_source,request.actual_exit_geometry_source,group});u.row.withdrawn=true;}
 std::sort(out.actual_exits.begin(),out.actual_exits.end(),[](const auto& a,const auto& z){return a.unit_ref<z.unit_ref;});return out;
});}
}
