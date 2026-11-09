#include "household_mobile_custody.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/naval_battle_state.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{value.error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){std::size_t n=0;for(const auto& row:c.state().facts.rows()){if(row.first==ref.id){if(row.second.id!=ref.id||row.second.domain!=ref.domain||row.second.kind!=ref.kind)fail("HouseholdMobileSourceHeaderMismatch",ref.id);return n;}++n;}fail("HouseholdMobileSourceHeaderMissing",ref.id);}
template<class T>const T* by_id(const std::vector<T>& rows,const StableId& id){const T* result=nullptr;for(const auto& v:rows)if(v.id==id){if(result)fail("DuplicateHouseholdMobileTitleSource",id);result=&v;}return result;}
bool same_moment(const WorldMoment&a,const WorldMoment&b){const auto c=compare_world_moments(a,b);return c.ok&&!c.value;}
bool same_source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
// Closed original SetInventoryOwner writers: family Estate succession, applied
// military occupation, and naval capture. No arbitrary resource command is an
// authorisation. This finite branch does not adopt dynamically transferred title.
void initial_title_before(const HouseholdPurchaseContext& h,const HouseholdMobileCustodyContext& x,
 const HouseholdPurchasePlan& p,const StableId& inventory,const SourceRef& cut,const WorldMoment& at){
 if(&x.spatial.common!=&h.common||&x.spatial.common_catalog!=&h.shared)fail("HouseholdMobileSnapshotMismatch");
 const auto* initial=h.shared.required_inventories.find(inventory);const auto* home=h.economy.households.find(p.household_id);
 if(!initial||!home||!owner_equal(initial->owner,{Owner::Kind::household,p.household_id,{}})||inventory==home->inventory_id||inventory==p.destination_inventory)fail("HouseholdMobileInitialPrivateContainerRequired",inventory);
 for(const auto& row:h.economy.cohorts.rows())if(row.second.household_group_id==p.household_id&&row.second.private_inventory_id==inventory)fail("HouseholdMobileHomeInventoryForbidden",inventory);
 const auto end=position(h.common,cut);
 // Rule out aliasing a ship hold or Estate container even before its first
// transfer; these have their own title sources, rather than portable-household semantics.
 for(const auto& e:x.estate_catalog.estates.rows())if(e.second.inventory_id==inventory)fail("HouseholdMobileEstateContainerMismatch",inventory);
 for(const auto& v:x.naval_catalog.vessels)if(v.cargo_inventory_id==inventory)fail("HouseholdMobileVesselContainerMismatch",inventory);
 for(const auto& row:x.family.facts.rows())if(const auto* f=std::get_if<FamilyChoice>(&row.second))if(f->kind=="family_succession"){
  const auto n=position(h.common,{"family","family_succession",row.first});
  if(!f->assets||std::count(x.family.successions.begin(),x.family.successions.end(),row.first)!=1)fail("HouseholdMobileSuccessionSourceIncomplete",row.first);
  if(n<end){if(f->tick<0||f->tick>at.half_day_tick)fail("HouseholdMobileTitleTimeMismatch",row.first);for(const auto& e:f->assets->estates)if(e.inventory_id==inventory)fail("HouseholdMobileDynamicTitleSourceRequired",row.first);}
 }
 for(const auto& id:x.family.successions){const auto* row=x.family.facts.find(id);const auto* f=row?std::get_if<FamilyChoice>(row):nullptr;if(!f||f->kind!="family_succession")fail("HouseholdMobileSuccessionSourceIncomplete",id);}
 for(const auto& f:x.military.occupation_facts){
  const auto n=position(h.common,{"military","military_occupation",f.id});
  if(!f.assets_applied.value_or(false))continue;
  if(!f.asset_manifest)fail("HouseholdMobileOccupationSourceIncomplete",f.id);
  if(n<end){if(f.tick<0||f.tick>at.half_day_tick)fail("HouseholdMobileTitleTimeMismatch",f.id);for(const auto& id:f.public_inventory_ids)if(id==inventory)fail("HouseholdMobileDynamicTitleSourceRequired",f.id);for(const auto& v:f.asset_manifest->inventories)if(v.first==inventory)fail("HouseholdMobileDynamicTitleSourceRequired",f.id);}
 }
 for(const auto& f:x.naval.battle.captures){
  const auto n=position(h.common,{"military_naval","naval_capture",f.id});const auto* vessel=by_id(x.naval_catalog.vessels,f.vessel_id);
  if(!vessel)fail("HouseholdMobileNavalOriginalVesselRequired",f.id);
  if(n<end){if(f.tick<0||f.tick>at.half_day_tick)fail("HouseholdMobileTitleTimeMismatch",f.id);if(vessel->cargo_inventory_id==inventory)fail("HouseholdMobileDynamicTitleSourceRequired",f.id);}
 }
 // Reverse header coverage prevents removing a typed owner application and then
// treating the surviving initial definition as proof that nothing changed.
 for(const auto& row:h.common.state().facts.rows()){
  const auto& f=row.second;
  if(f.domain=="family"&&f.kind=="family_succession"){const auto* v=x.family.facts.find(row.first);const auto* s=v?std::get_if<FamilyChoice>(v):nullptr;if(!s||s->kind!="family_succession")fail("HouseholdMobileTitlePayloadMissing",row.first);}
  if(f.domain=="military"&&f.kind=="military_occupation"&&!by_id(x.military.occupation_facts,row.first))fail("HouseholdMobileTitlePayloadMissing",row.first);
  if(f.domain=="military_naval"&&f.kind=="naval_capture"&&!by_id(x.naval.battle.captures,row.first))fail("HouseholdMobileTitlePayloadMissing",row.first);
 }
}
void original_plan(const HouseholdPurchaseContext& h,const HouseholdPurchasePlan& p,const SourceRef& source,const WorldMoment& at){
 if(h.selected_household_version!=3||!same_source_ref(source,{"local_economy","household_purchase_planned",p.plan_id})||!std::holds_alternative<CohortHouseholdBuyer>(p.execution_choice)||cmp(at,h.now)>0)fail("HouseholdMobilePlanSourceMismatch",p.plan_id);
 const auto* policy=h.state.policies.find(p.accepted_policy_ref.id);
 if(!policy||!same_source_ref(policy->acceptance_source_ref,p.accepted_policy_ref)||policy->household_id!=p.household_id||policy->revision!=p.accepted_policy_revision||at.half_day_tick<policy->valid_from_tick||at.half_day_tick>=policy->valid_until_tick||at.half_day_tick>p.latest_start_tick||p.delivery_deadline>policy->valid_until_tick)fail("HouseholdMobileOriginalPolicyRequired",p.plan_id);
 if(position(h.common,policy->acceptance_source_ref)>=position(h.common,source))fail("HouseholdMobilePolicyNotYetAccepted",p.plan_id);
 take(h.controls.validate_accepted(p.controller_authority_ref,at.half_day_tick));
}
}
bool same_household_mobile_custody(const std::optional<HouseholdMobileCustodyAdoption>& a,const std::optional<HouseholdMobileCustodyAdoption>& b){if(bool(a)!=bool(b))return false;if(!a)return true;const auto&x=a->actual_view;const auto&y=b->actual_view;return same_source_ref(a->plan_source,b->plan_source)&&x.inventory_id==y.inventory_id&&x.vehicle_ref==y.vehicle_ref&&same_source(x.original_container_source,y.original_container_source)&&same_source(x.spatial_source,y.spatial_source)&&same_source(x.capacity_source,y.capacity_source)&&same_moment(x.adopted_at,y.adopted_at);}
Outcome<HouseholdMobileCustodyAdoption> capture_household_mobile_custody(const HouseholdPurchaseContext& h,const HouseholdMobileCustodyContext& x,const HouseholdPurchasePlan& p,const HouseholdMobileCustodyRequest& request,const SourceRef& source){try{
 if(request.inventory_id.empty()||p.mobile_custody_adoption||cmp(h.now,take(x.spatial.clock.read_current_moment())))fail("HouseholdMobileCaptureSourceMismatch",p.plan_id);
 original_plan(h,p,source,h.now);initial_title_before(h,x,p,request.inventory_id,source,h.now);
 const auto* current=h.common.inventory(request.inventory_id);if(!current||!owner_equal(current->owner,{Owner::Kind::household,p.household_id,{}}))fail("HouseholdMobileCurrentTitleMismatch",request.inventory_id);
 auto view=take(capture_mobile_inventory_adoption(x.spatial,request.inventory_id,source,h.now));
 return {true,{source,std::move(view)},{}};
 }catch(const Failure&e){return {false,{},e.error};}}
Status validate_household_mobile_custody(const HouseholdPurchaseContext& h,const HouseholdMobileCustodyContext& x,const HouseholdPurchasePlan& p){try{
 if(!p.mobile_custody_adoption)return common_success();const auto& a=*p.mobile_custody_adoption;
 original_plan(h,p,a.plan_source,a.actual_view.adopted_at);initial_title_before(h,x,p,a.actual_view.inventory_id,a.plan_source,a.actual_view.adopted_at);
 take(validate_mobile_inventory_adoption(x.spatial,a.actual_view,a.plan_source,a.actual_view.adopted_at));
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
}
