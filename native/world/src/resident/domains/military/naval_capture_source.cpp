#include "resident/domains/military/military_model.hpp"
// 不从当前Owner倒推旧批准，只消费本候选已经实际登记的唯一naval_capture及其原载荷清单。
#include "naval_capture_source.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 原对象缺失即拒绝，没有默认收款人、空仓库或第二本待清算资产账。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool same_owner(const Owner& a,const Owner& b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
bool same_assignments(const std::vector<NavalVesselAssignment>& a,const std::vector<NavalVesselAssignment>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].vessel_id!=b[i].vessel_id||a[i].unit_id!=b[i].unit_id||a[i].passenger_ids!=b[i].passenger_ids||a[i].prisoner_ids!=b[i].prisoner_ids||a[i].extra_inventory_ids!=b[i].extra_inventory_ids)return false;return true;}
Outcome<NavalAppliedCaptureSource> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
}
// 冻结的after事实只能在其真实应用点读取；以后交易/二次转属须另沿历史责任接口证明。
Outcome<NavalAppliedCaptureSource> naval_capture_applied_in_candidate(const CommonRead& common,const MilitaryState& military,const MilitaryAccess& access,const NavalState& naval,const StableId& id){
 const auto* fact=find(naval.battle.captures,id);if(!fact||fact->tick!=common.tick()||!access.fact_exists||!access.fact_exists(id)||!access.fact_exists(fact->round_source_id))return fail("NavalAppliedCaptureSourceRequired",id);const auto* round=find(naval.battle.rounds,fact->round_source_id);if(!round||round->battle_id!=fact->battle_id||round->tick!=fact->tick)return fail("NavalAppliedCaptureSourceMismatch",id);
 const auto* vessel=find(naval.vessels,fact->vessel_id);const auto* winner=find(naval.fleets,fact->to_fleet_id);const auto* loser=find(naval.fleets,fact->from_fleet_id);const auto* winning_army=winner?find(military.armies,winner->army_id):nullptr;const auto* losing_army=loser?find(military.armies,loser->army_id):nullptr;const auto* hold=vessel?common.inventory(vessel->cargo_inventory_id):nullptr;
 if(!vessel||!winner||!loser||!winning_army||!losing_army||vessel->fleet_id!=winner->id||!same_owner(vessel->owner,fact->to_owner)||!hold||!same_owner(hold->owner,fact->to_owner)||!same_assignments(winner->assignments,fact->after_assignments)||!same_assignments(loser->assignments,fact->from_after_assignments)||fact->to_owner.kind!=Owner::Kind::organization||winning_army->organization_id!=fact->to_owner.id||!((fact->from_owner.kind==Owner::Kind::organization&&losing_army->organization_id==fact->from_owner.id)||(fact->from_owner.kind==Owner::Kind::actor&&loser->commander_actor_id==fact->from_owner.id)))return fail("NavalAppliedCaptureAssetMismatch",id);
 bool original_binding=false,current_binding=false;for(const auto& a:fact->from_before_assignments)if(a.vessel_id==fact->vessel_id)for(const auto& claim:round->claims)if(claim.unit_id==a.unit_id&&claim.captor_unit_id==fact->captor_unit_id&&claim.captor_army_id==winner->army_id)original_binding=true;for(const auto& a:winner->assignments)if(a.vessel_id==fact->vessel_id&&a.unit_id==fact->captor_unit_id)current_binding=true;if(!original_binding||!current_binding)return fail("NavalAppliedCaptureClaimMismatch",id);
 std::set<StableId> seen;for(const auto& cargo:fact->cargo){if(!seen.insert(cargo.lot_id).second||cargo.quantity<1)return fail("NavalAppliedCaptureCargoMismatch",cargo.lot_id);const auto* actual=find(hold->lots,cargo.lot_id);if(!actual||actual->item_id!=cargo.item_id||actual->quantity!=cargo.quantity||!same_owner(actual->owner,cargo.to_owner)||!actual->reserved_by.empty())return fail("NavalAppliedCaptureCargoMismatch",cargo.lot_id);if(cargo.source_inventory_id!=hold->id){const auto* original=common.inventory(cargo.source_inventory_id);if(!original||find(original->lots,cargo.lot_id))return fail("NavalAppliedCaptureCargoNotMoved",cargo.lot_id);}}
 if(seen.size()!=hold->lots.size())return fail("NavalAppliedCaptureCargoUnrecorded",hold->id);for(const auto& inv:fact->emptied_inventory_ids){const auto* original=common.inventory(inv);if(!original||!original->lots.empty())return fail("NavalAppliedCaptureCargoNotMoved",inv);}
 NavalAppliedCaptureSource out;out.capture_id=id;out.round_source_id=fact->round_source_id;out.actor_id=fact->actor_id;out.vessel_id=vessel->id;out.inventory_id=hold->id;out.from_army_id=loser->army_id;out.to_army_id=winner->army_id;out.authority_source=fact->authority_source;out.tick=fact->tick;out.from_owner=fact->from_owner;out.to_owner=fact->to_owner;out.cargo=fact->cargo;return {true,std::move(out),{}};
}
}
