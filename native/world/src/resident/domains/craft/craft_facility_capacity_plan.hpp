// Private S9 decision algorithm, shared by hot adoption and immutable replay.
// Inputs have already been read from the original owners; no command exposes it.
#pragma once
#include "craft_facility_boundary.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::craft_capacity_plan {
struct Failure{RuleError error;};
[[noreturn]]inline void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
inline int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
inline bool overlaps(const MomentSpan&a,const MomentSpan&b){return compare(a.from,b.to)<0&&compare(b.from,a.to)<0;}
inline bool covers(const std::vector<MomentSpan>&parts,const MomentSpan&window){auto at=window.from;for(const auto&part:parts){if(compare(part.from,part.to)>=0)fail("CraftFacilityInvalidAvailability");if(compare(part.to,at)<=0)continue;if(compare(part.from,at)>0)return false;if(compare(part.to,at)>0)at=part.to;if(compare(at,window.to)>=0)return true;}return false;}
inline bool physical_equal(const FacilityPhysicalUnitRef&a,const FacilityPhysicalUnitRef&b){if(a.facility_id!=b.facility_id||a.unit_key!=b.unit_key||a.installation_source_ref.index()!=b.installation_source_ref.index())return false;if(const auto*i=std::get_if<InstalledPhysicalUnitSource>(&a.installation_source_ref)){const auto&j=std::get<InstalledPhysicalUnitSource>(b.installation_source_ref);return i->source_schema==j.source_schema&&same_source_ref(i->source_ref,j.source_ref);}const auto&i=std::get<ActivatedPhysicalUnitSource>(a.installation_source_ref).original_facility_activation_receipt_ref;const auto&j=std::get<ActivatedPhysicalUnitSource>(b.installation_source_ref).original_facility_activation_receipt_ref;return i.receipt_id==j.receipt_id&&i.source_schema==j.source_schema;}
struct Prior {MomentSpan window;Integer claimed_units=0;bool retained=false;std::vector<FacilityPhysicalUnitRef> units;};
inline bool fits(const EconomicFacilityServiceCapacityView&cap,const EconomicFacilitySlotDefinition&d,const MomentSpan&window,const std::vector<FacilityPhysicalUnitRef>&selected,const std::vector<Prior>&earlier){if(cap.capacity<0)fail("CraftNegativeFacilityCapacity");if(std::holds_alternative<AggregateFacilityLayout>(d.physical_layout)){if(!selected.empty())fail("AggregateCraftPhysicalSelection");}else{if(selected.size()!=1)return false;for(const auto&chosen:selected)if(std::none_of(cap.physical_units.begin(),cap.physical_units.end(),[&](const auto&available){return physical_equal(chosen,available);}))return false;}if(!covers(cap.availability_intervals,window))return false;std::vector<WorldMoment>cuts{window.from,window.to};for(const auto&row:earlier){if(compare(window.from,row.window.from)<0&&compare(row.window.from,window.to)<0)cuts.push_back(row.window.from);if(compare(window.from,row.window.to)<0&&compare(row.window.to,window.to)<0)cuts.push_back(row.window.to);if(overlaps(window,{row.window.from,row.window.to}))for(const auto&a:selected)for(const auto&b:row.units)if(physical_equal(a,b))return false;}std::sort(cuts.begin(),cuts.end(),[](const auto&a,const auto&b){return compare(a,b)<0;});cuts.erase(std::unique(cuts.begin(),cuts.end(),[](const auto&a,const auto&b){return !compare(a,b);}),cuts.end());for(std::size_t n=0;n+1<cuts.size();++n){Integer count=1;for(const auto&row:earlier)if(overlaps({cuts[n],cuts[n+1]},{row.window.from,row.window.to})&&(!row.retained||!std::holds_alternative<IndependentFacilityUnits>(d.physical_layout))){if(row.claimed_units<1||count>std::numeric_limits<Integer>::max()-row.claimed_units)fail("CraftFacilityCapacityOverflow");count+=row.claimed_units;}if(count>cap.capacity)return false;}return true;}

enum class Disposition {unchanged,readmitted,suspended,retained,cleared};
struct Input {
 const FacilityClaim2& old;const WorldMoment& at;
 const EconomicFacilityServiceCapacityView& capacity;
 const EconomicFacilitySlotDefinition& definition;
 const CraftFacilityEligible* eligible=nullptr;
 bool working=false,was_retained=false,uncleared=false,cleared=false,whole_closed=false;
 const std::vector<FacilityPhysicalUnitRef>& previous_units;
 const std::vector<Prior>& earlier;
 const std::vector<FacilityPhysicalUnitRef>& later_actual_placements;
 Integer shared_max_claim_half_days=0;
};
struct Decision {Disposition kind=Disposition::suspended;std::optional<MomentSpan> window;std::vector<FacilityPhysicalUnitRef> physical_units;};
inline Outcome<Decision> choose(const Input&i){try{
 const auto&old=i.old;const auto&cap=i.capacity;const auto&d=i.definition;
 if(old.units!=1||old.slot_ref!=cap.slot_ref||compare(cap.occurred_at,i.at)>0)fail("CraftCapacityPlanOriginalScopeMismatch",old.claim_id);
 if(!i.whole_closed&&i.working&&i.eligible&&!i.was_retained&&compare(old.until,i.at)>0&&same_source_ref(old.usage_right_ref,cap.usage_right_ref)&&fits(cap,d,{compare(old.from,i.at)<0?i.at:old.from,old.until},i.previous_units,i.earlier))return {true,{Disposition::unchanged,{},{}},{}};
 if(i.cleared)return {true,{Disposition::cleared,{},{}},{}};
 if(!i.whole_closed&&i.working&&i.eligible&&i.eligible->accepted_service_window&&!i.was_retained){
  const auto&window=*i.eligible->accepted_service_window;const auto bound=std::min(d.max_claim_half_days,i.shared_max_claim_half_days);
  if(bound<1||compare(window.from,i.at)||compare(window.from,window.to)>=0||window.from.half_day_tick>std::numeric_limits<Integer>::max()-bound)fail("CraftOriginalFacilityWindowMismatch",old.claim_id);
  auto until=window.from;until.half_day_tick+=bound;if(compare(window.to,until)>0)fail("CraftOriginalFacilityWindowExceeded",old.claim_id);
  if(std::holds_alternative<AggregateFacilityLayout>(d.physical_layout)){if(fits(cap,d,window,{},i.earlier))return {true,{Disposition::readmitted,window,{}},{}};}
  else if(i.uncleared){if(fits(cap,d,window,i.previous_units,i.earlier))return {true,{Disposition::readmitted,window,i.previous_units},{}};}
  else for(const auto&unit:cap.physical_units){bool busy=false;for(const auto&used:i.later_actual_placements)if(physical_equal(unit,used))busy=true;if(!busy&&fits(cap,d,window,{unit},i.earlier))return {true,{Disposition::readmitted,window,{unit}},{}};}
 }
 if(i.uncleared){const auto bound=std::min(d.max_claim_half_days,i.shared_max_claim_half_days);if(d.retention_review_halfdays<1||d.retention_review_halfdays>bound||i.at.half_day_tick>std::numeric_limits<Integer>::max()-d.retention_review_halfdays)fail("CraftFacilityRetentionWindowRequired",old.claim_id);auto until=i.at;until.half_day_tick+=d.retention_review_halfdays;return {true,{Disposition::retained,MomentSpan{i.at,until},i.previous_units},{}};}
 return {true,{Disposition::suspended,{},{}},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
