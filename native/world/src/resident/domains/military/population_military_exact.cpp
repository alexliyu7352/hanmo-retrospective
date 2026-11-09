// The original population-military row consumes one real precise military
// source. Population counts, labor and provision remain in the S5 writer.
#include "population_military_internal.hpp"
#include "dynamic_land_consequence.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
using namespace population_military_detail;
using S=PopulationSoldierStatus;
WorldMoment actual_now(const PopulationMilitaryReadContext& c){
  if (take(selected_population_military_version(c.catalog,
                                                c.actual_integration)) != 4 ||
      c.selected_version != 4 || !c.military_exact || !c.actual_clock ||
      !c.dynamic_land_sources)
    fail("PopulationMilitaryExactSourcesRequired");
  auto now = take(c.actual_clock->read_current_moment());
  take(encode_world_moment(now));
  auto low = take(normalize_world_moment(c.common.tick(), {}));
  if (take(compare_world_moments(now, low)) < 0)
    fail("PopulationMilitaryPhaseMomentMismatch");
  if (c.common.tick() != INT64_MAX) {
    auto high = take(normalize_world_moment(c.common.tick() + 1, {}));
    if (take(compare_world_moments(now, high)) > 0)
      fail("PopulationMilitaryPhaseMomentMismatch");
  }
 if(c.phase_finish_tick&&(c.common.tick()==INT64_MAX||*c.phase_finish_tick!=c.common.tick()+1))fail("PopulationMilitaryPhaseBoundaryMismatch");
 return now;
}
const DynamicMilitaryCasualtyFact2& exact_source(const PopulationMilitaryReadContext& c,const PopulationMilitaryFact& f){
 auto now=actual_now(c);if(f.kind!="population_casualty"||f.military_source_schema!=2||!f.occurred_at)fail("PopulationMilitaryExactCasualtyRequired",f.id);
 const auto* source=c.military_exact->dynamic_casualty_facts.find(f.military_receipt_id);
 const auto* header=c.common.state().facts.find(f.military_receipt_id);
 if(!source||source->id!=f.military_receipt_id||!header||header->domain!="military"||header->kind!="military_casualties"||find(c.military.casualty_facts,f.military_receipt_id)||!same_moment(source->effective_at,*f.occurred_at,{})||f.tick!=source->effective_at.half_day_tick||take(compare_world_moments(source->effective_at,now))>0)fail("PopulationMilitaryExactCasualtyMismatch",f.id);
 DynamicMilitaryLandConsequenceContext source_context{c.military_catalog,*c.dynamic_land_sources,*c.actual_clock,&c.state,&c.catalog,{}};
 take(validate_dynamic_military_land_consequence(c.common,c.military,*c.military_exact,source_context,{"military","military_casualties",source->id}));
 return *source;
}
bool earlier(const CommonStore& common,const StableId& a,const StableId& b){bool seen=false;for(const auto& id:common.state().facts.order()){if(id==b)return seen;if(id==a)seen=true;}return false;}
PopulationSoldierStatus roster_status(const MilitaryRosterReceipt& r,const StableId& id){
 if(std::count(r.member_ids.begin(),r.member_ids.end(),id)!=1)fail("PopulationMilitaryCasualtyMember",id);
 Integer count=0;S result=S::released;
 for(const auto& group:std::vector<std::pair<const std::vector<StableId>*,S>>{{&r.fit,S::fit},{&r.wounded,S::wounded},{&r.dead,S::dead},{&r.captured,S::captured},{&r.deserted,S::deserted}}){auto n=std::count(group.first->begin(),group.first->end(),id);count+=n;if(n)result=group.second;}
 if(count!=1)fail("PopulationMilitaryCasualtyClassification",id);return result;
}
}
Outcome<Integer> read_population_military_boundary_tick(const PopulationMilitaryReadContext& c){return movement_detail::guard<Integer>([&]{
 const auto selected=movement_detail::take(selected_population_military_version(c.catalog,c.actual_integration));
 if ((selected == 4) != (c.selected_version == 4))
   movement_detail::fail("PopulationMilitarySelectedContextMismatch");
 if (c.selected_version == 4)
   return actual_now(c).half_day_tick;
 if(c.selected_version!=1&&c.selected_version!=2)movement_detail::fail("UnsupportedPopulationMilitaryVersion");
 if(c.phase_finish_tick){if(c.common.tick()==INT64_MAX||*c.phase_finish_tick!=c.common.tick()+1)movement_detail::fail("PopulationMilitaryPhaseBoundaryMismatch");return *c.phase_finish_tick;}return c.common.tick();
});}
Status validate_population_military_fact_source_tail(const PopulationMilitaryReadContext& c,const PopulationMilitaryFact& f){return movement_detail::status([&]{
 if(f.military_source_schema==1){if(f.occurred_at)movement_detail::fail("LegacyPopulationMilitaryHasExactMoment",f.id);return;}
 if (c.selected_version != 4 || f.military_source_schema != 2 ||
     f.kind != "population_casualty" || !f.occurred_at)
   movement_detail::fail("PopulationMilitaryExactSourceVersionRequired", f.id);
 exact_source(c,f);
});}
Outcome<Integer> read_population_military_event_source_schema(const PopulationMilitaryReadContext& c,const SourceRef& ref){return movement_detail::guard<Integer>([&]()->Integer{
 const auto* row=ref.domain=="population_military"?c.state.facts.find(ref.id):nullptr;const auto* head=c.common.state().facts.find(ref.id);if(!row||row->id!=ref.id||row->kind!=ref.kind||!head||head->domain!=ref.domain||head->kind!=ref.kind)movement_detail::fail("PopulationMilitaryActualEventSourceRequired",ref.id);
 movement_detail::take(validate_population_military_fact_source_tail(c,*row));return row->military_source_schema==2?2:1;
});}
Outcome<WorldMoment> read_exact_population_military_event_moment(const PopulationMilitaryReadContext& c,const SourceRef& ref,Integer schema){return movement_detail::guard<WorldMoment>([&]{
 if(schema!=2||movement_detail::take(read_population_military_event_source_schema(c,ref))!=2)movement_detail::fail("PopulationMilitaryExactEventSourceRequired",ref.id);const auto& row=*c.state.facts.find(ref.id);const auto& original=exact_source(c,row);if(!earlier(c.common,original.id,row.id))movement_detail::fail("PopulationMilitaryActualEventSourceOrder",row.id);return *row.occurred_at;
});}
Outcome<PopulationMilitaryCasualtyRead> read_population_military_casualty(const PopulationMilitaryReadContext& c,const PopulationMilitaryFact& f){return movement_detail::guard<PopulationMilitaryCasualtyRead>([&]{
 if(f.kind!="population_casualty")movement_detail::fail("PopulationMilitaryCasualtyRequired",f.id);
 movement_detail::take(validate_population_military_fact_source_tail(c,f));
 if(f.military_source_schema==2){const auto& source=exact_source(c,f);return PopulationMilitaryCasualtyRead{&source.roster_receipts,source.effective_at,2};}
 movement_detail::take(validate_population_military_casualty_source(c,f.military_receipt_id));
 const auto* old=population_military_detail::find(c.military.casualty_facts,f.military_receipt_id);
 if(!old||old->tick!=f.tick||(c.military_exact&&c.military_exact->dynamic_casualty_facts.find(f.military_receipt_id)))movement_detail::fail("PopulationMilitaryCasualtySourceMismatch",f.id);
 return PopulationMilitaryCasualtyRead{&old->units,{},1};
});}
Outcome<PopulationMilitaryEffects> consume_exact_population_military_casualty_in_candidate(PopulationMilitaryWriteContext& c,const SourceRef& source){return movement_detail::guard<PopulationMilitaryEffects>([&]{
 if(!c.catalog.enabled)movement_detail::fail("PopulationMilitaryCapabilityRequired");
 if(source.domain!="military"||source.kind!="military_casualties")movement_detail::fail("PopulationMilitaryExactCasualtyRequired",source.id);
 PopulationMilitaryFact fact;fact.kind="population_casualty";fact.military_receipt_id=source.id;fact.military_source_schema=2;
 if (c.selected_version != 4 || !c.military_exact)
   movement_detail::fail("PopulationMilitaryExactSourcesRequired");
 const auto* original=c.military_exact->dynamic_casualty_facts.find(source.id);if(!original)movement_detail::fail("PopulationMilitaryExactCasualtyRequired",source.id);
 fact.occurred_at=original->effective_at;fact.tick=original->effective_at.half_day_tick;
 auto read=c.read();auto rosters=movement_detail::take(read_population_military_casualty(read,fact));
 for(const auto& row:c.state.facts.rows())if(row.second.military_receipt_id==source.id){
  if(row.second.kind!=fact.kind||row.second.military_source_schema!=2||!row.second.occurred_at||!movement_detail::same_moment(*row.second.occurred_at,*fact.occurred_at,{})||!earlier(c.common,source.id,row.first))movement_detail::fail("PopulationMilitaryCasualtyReplayMismatch",row.first);
  auto applied=movement_detail::take(apply_population_military_fact(c,row.first));PopulationMilitaryEffects out;out.fact_ids.push_back(row.first);out.receipt_ids.push_back(row.first);out.fact_ids.insert(out.fact_ids.end(),applied.fact_ids.begin(),applied.fact_ids.end());out.receipt_ids.insert(out.receipt_ids.end(),applied.receipt_ids.begin(),applied.receipt_ids.end());return out;
 }
 auto now=actual_now(read);if(!movement_detail::same_moment(now,*fact.occurred_at,{}))movement_detail::fail("PopulationMilitaryCasualtyAdoptionMomentMismatch",source.id);
 for(const auto& roster:*rosters.units)for(const auto& enrollment:c.state.enrollments.rows())if(enrollment.second.unit_id==roster.unit_id&&earlier(c.common,enrollment.first,source.id)){
  const auto* definition=c.catalog.sources.find(enrollment.second.source_id);if(!definition)movement_detail::fail("UnknownPopulationMilitarySource",enrollment.second.source_id);
  for(const auto& member:enrollment.second.members){if(!population_military_detail::has(roster.member_ids,member.member_id))continue;
   const auto after=roster_status(roster,member.member_id);const auto before=population_military_detail::recorded_status(c.state,member.member_id);if(before==after)continue;
   if(before==S::released||before==S::dead||before==S::captured||before==S::deserted||(before==S::wounded&&after==S::fit))movement_detail::fail("PopulationMilitaryCasualtyReversal",member.member_id);
   StableId captor;if(after==S::captured){Integer count=0;for(const auto& pair:roster.captors)if(pair.first==member.member_id){captor=pair.second;++count;}if(count!=1||captor.empty())movement_detail::fail("PopulationMilitaryCaptorMissing",member.member_id);}
   fact.movements.push_back({member.member_id,enrollment.first,population_military_detail::cohort(*definition,before),population_military_detail::cohort(*definition,after),captor,before,after});
  }
 }
 auto id=movement_detail::take(c.common.next_id("population_military"));fact.id=id;movement_detail::take(c.common.append_fact({id,fact.kind,"population_military"}));
 if(!c.state.facts.insert(id,std::move(fact)))movement_detail::fail("DuplicatePopulationMilitaryFact",id);
 auto applied=movement_detail::take(apply_population_military_fact(c,id));PopulationMilitaryEffects out;out.fact_ids.push_back(id);out.receipt_ids.push_back(id);out.fact_ids.insert(out.fact_ids.end(),applied.fact_ids.begin(),applied.fact_ids.end());out.receipt_ids.insert(out.receipt_ids.end(),applied.receipt_ids.begin(),applied.receipt_ids.end());return out;
});}
}
