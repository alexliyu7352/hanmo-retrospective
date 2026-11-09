#include "resident/storage/common_store.hpp"
#include "occupation_time.hpp"
#include "military_roster_history.hpp"
#include "occupation_rules.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <set>
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){const T* out=nullptr;for(const auto& row:rows)if(row.id==id){if(out)fail("MilitaryOccupationSourceAmbiguous",id);out=&row;}return out;}
void header(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind||h->id!=ref.id)fail("MilitaryOccupationTriggerSourceRequired",ref.id);}
void trigger(const CommonStore& common,const MilitaryState& state,const MilitaryCatalog& catalog,const MilitaryOccupationFact& fact,const MilitaryOccupationTime2& time,ExactArithmeticBudget b){
 const auto* siege=find(state.sieges,fact.siege_id);const auto* stronghold=find(catalog.strongholds,fact.stronghold_id);const auto* campaign=find(state.campaigns,fact.campaign_id);
 if(!siege||!campaign||campaign->army_id!=fact.army_id||!siege->result||(siege->phase!="result"&&siege->phase!="closed")||siege->stronghold_id!=fact.stronghold_id||siege->campaign_id!=fact.campaign_id||!stronghold||stronghold->garrison_id!=fact.garrison_id||siege->result->winner_side!=0||siege->result->tick!=fact.tick||time.actual_occupation_at.half_day_tick!=fact.tick)fail("MilitaryOccupationActualResultRequired",fact.id);
 take(encode_world_moment(time.actual_occupation_at,b));
 const auto& result=*siege->result;
 if(time.producer_kind==MilitaryOccupationProducerKind::calendar_siege_results){
  if(result.kind!="occupied"||result.terms||compare(time.actual_occupation_at.sub_half_day,{},b)!=0||!same_source(time.trigger_source,{"military","siege_result",siege->id}))fail("MilitaryOccupationCalendarSourceMismatch",fact.id);
 }else if(time.producer_kind==MilitaryOccupationProducerKind::strategy){
  if(result.kind!="surrendered"||!result.terms||(*result.terms!="safe_departure"&&*result.terms!="integrate")||!same_source(time.trigger_source,{"military","siege_result",siege->id}))fail("MilitaryOccupationStrategySourceMismatch",fact.id);
  for(const auto& transfer:state.integration_facts)if(transfer.siege_id==siege->id)fail("MilitaryOccupationIntegrationSourceRequired",fact.id);
 }else if(time.producer_kind==MilitaryOccupationProducerKind::integration_transfer){
  const auto* transfer=find(state.integration_facts,time.trigger_source.id);
  if(time.trigger_source.domain!="military"||time.trigger_source.kind!="military_integration"||!transfer||transfer->siege_id!=siege->id||transfer->tick!=fact.tick||transfer->recipient_army_id!=fact.army_id||transfer->recipient_organization_id!=fact.to_organization_id||result.kind!="surrendered"||result.terms.value_or("")!="integrate")fail("MilitaryOccupationIntegrationSourceMismatch",fact.id);
  header(common,time.trigger_source);
  const auto cause=take(read_actual_common_fact_position(common,time.trigger_source));if(common.state().facts.find(fact.id)&&cause>=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_occupation",fact.id})))fail("MilitaryOccupationTriggerOrderMismatch",fact.id);
 }else fail("MilitaryOccupationProducerKindMismatch",fact.id);
}
}
Outcome<MilitaryOccupationTime2> capture_military_occupation_time(const CommonRead& common,const MilitaryState& state,const MilitaryCatalog& catalog,const MilitaryAccess& access,const MilitaryOccupationFact& fact,const MilitaryOccupationExecution& execution){return guard<MilitaryOccupationTime2>([&]{
 if(access.occupation_source_schema!=2||!access.occupation_clock||!access.occupation_common||static_cast<const CommonRead*>(access.occupation_common)!=&common)fail("MilitaryOccupationActualClockRequired",fact.id);
 const auto& store=*access.occupation_common;
 if(fact.exact_time||store.state().facts.find(fact.id))fail("MilitaryOccupationAlreadyAdopted",fact.id);
 for(const auto& previous:state.occupation_facts)if(previous.siege_id==fact.siege_id)fail("MilitaryOccupationAlreadyAdopted",fact.siege_id);
 MilitaryOccupationTime2 out;out.actual_occupation_at=take(access.occupation_clock->read_current_moment());out.producer_kind=execution.producer_kind;
 if(execution.producer_kind==MilitaryOccupationProducerKind::integration_transfer){if(execution.integration_transfer_id.empty())fail("MilitaryOccupationIntegrationSourceRequired",fact.id);out.trigger_source={"military","military_integration",execution.integration_transfer_id};}
 else {if(!execution.integration_transfer_id.empty())fail("MilitaryOccupationUnexpectedIntegrationSource",fact.id);out.trigger_source={"military","siege_result",fact.siege_id};}
 trigger(store,state,catalog,fact,out,{});return out;
});}
namespace {
WorldMoment actual_moment(const CommonStore& common,const MilitaryState& state,const MilitaryCatalog& catalog,const MilitaryOccupationFact& fact,const WorldMoment& current,ExactArithmeticBudget b){
 if(!fact.exact_time||fact.phase!="completed"||!fact.assets_applied.value_or(false)||!fact.asset_manifest||!fact.custody_transfers||!fact.disabled_office_ids||!fact.actor_changes)fail("MilitaryOccupationActualMomentRequired",fact.id);
 Integer same_siege=0;for(const auto& row:state.occupation_facts)if(row.siege_id==fact.siege_id)++same_siege;if(same_siege!=1)fail("MilitaryOccupationSourceAmbiguous",fact.id);
 trigger(common,state,catalog,fact,*fact.exact_time,b);if(take(compare_world_moments(fact.exact_time->actual_occupation_at,current,b))>0)fail("MilitaryOccupationFutureSource",fact.id);return fact.exact_time->actual_occupation_at;
}
}
Outcome<WorldMoment> read_military_occupation_actual_moment(const CommonStore& common,const MilitaryState& state,const MilitaryCatalog& catalog,const SourceRef& ref,Integer schema,const WorldMoment& current,ExactArithmeticBudget b){return guard<WorldMoment>([&]{
 if(ref.domain!="military"||ref.kind!="military_occupation"||schema!=2)fail("MilitaryOccupationExactSourceRequired",ref.id);header(common,ref);const auto* fact=find(state.occupation_facts,ref.id);
 if(!fact)fail("MilitaryOccupationActualMomentRequired",ref.id);return actual_moment(common,state,catalog,*fact,current,b);
});}
static Outcome<MilitaryGarrisonOwnershipBeforeSource> garrison_owner_source(const CommonStore& common,const MilitaryState& state,const MilitaryCatalog& catalog,const MilitaryAccess& access,const StableId& garrison,std::optional<SourceRefBuildView> boundary,const WorldMoment& at,const WorldMoment& now,Integer selected,ExactArithmeticBudget b,const MilitaryActualSourceWindow* window){return guard<MilitaryGarrisonOwnershipBeforeSource>([&]{
 if(selected!=1&&selected!=2)fail("UnsupportedMilitaryOccupationSchema");
 take(encode_world_moment(now,b));take(encode_world_moment(at,b));const auto compared=take(compare_world_moments(at,now,b));
 if(compared>0||(!boundary&&compared!=0))fail("MilitaryOwnershipActualBoundaryRequired",garrison);
 const auto edge=boundary?take(read_actual_common_fact_position(common,*boundary)):common.state().facts.order().size();
 const auto* camp=find(catalog.garrisons,garrison);if(!camp)fail("UnknownGarrison",garrison);
 // Reuse the original applied receipt/asset/predecessor validator. Its integer
 // upper projection is not the occurrence of a selected exact occupation.
 const auto chain=take(military_garrison_owner(common,state,catalog,access,garrison,now.half_day_tick));
 MilitaryGarrisonOwnershipBeforeSource out{camp,nullptr,1};std::optional<std::size_t> previous_position,best;std::optional<WorldMoment> previous_at;
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> seen;
 for(const auto& id:chain.chain){
  const auto* event=find(state.occupation_facts,id);if(!event||event->garrison_id!=garrison)fail("MilitaryOwnershipChainMismatch",id);
  const auto index=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_occupation",id}));
  if(!seen.insert(id).second||(previous_position&&index>=*previous_position))fail("MilitaryOwnershipSourceOrderMismatch",id);
  Integer schema=1;WorldMoment occurred;
  if(event->exact_time){if(selected!=2)fail("UnexpectedMilitaryOccupationExactSource",id);schema=2;occurred=actual_moment(common,state,catalog,*event,now,b);if(window)take(validate_military_source_in_actual_window(common,SourceRefBuildView{"military","military_occupation",id},occurred,now,window,b));}
  else occurred=take(normalize_world_moment(event->tick,{},b));
  if(take(compare_world_moments(occurred,now,b))>0||(previous_at&&take(compare_world_moments(occurred,*previous_at,b))>0))fail("MilitaryOwnershipSourceClockMismatch",id);
  if(index<edge){if(take(compare_world_moments(occurred,at,b))>0)fail("MilitaryOwnershipHistoryBoundary",id);if(!best||index>*best){best=index;out.actual_occupation=event;out.occupation_source_schema=schema;}}
  else if(take(compare_world_moments(occurred,at,b))<0)fail("MilitaryOwnershipHistoryBoundary",id);
  previous_position=index;previous_at=std::move(occurred);
 }
 for(const auto& event:state.occupation_facts)if(event.garrison_id==garrison&&!seen.count(event.id))fail("MilitaryOwnershipChainIncomplete",event.id);
 return out;
});}
Outcome<MilitaryGarrisonOwnershipBeforeSource> read_military_garrison_owner_before_actual(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& catalog,const MilitaryAccess& access,const StableId& garrison,std::optional<SourceRefBuildView> boundary,const WorldMoment& at,const WorldMoment& now,Integer selected,ExactArithmeticBudget budget){return garrison_owner_source(c,s,catalog,access,garrison,boundary,at,now,selected,budget,nullptr);}
Outcome<MilitaryGarrisonOwnershipBeforeSource> read_military_garrison_owner_before_actual(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& catalog,const MilitaryAccess& access,const StableId& garrison,std::optional<SourceRefBuildView> boundary,const WorldMoment& at,const WorldMoment& now,Integer selected,ExactArithmeticBudget budget,const MilitaryActualSourceWindow* window){if(window){auto valid=validate_military_actual_source_window(c,window->outer_prefix,now,window,budget);if(!valid.ok)return {false,{},valid.error};}return garrison_owner_source(c,s,catalog,access,garrison,boundary,at,now,selected,budget,window);}
Status validate_military_occupation_times(const CommonStore& common,const MilitaryState& state,const MilitaryCatalog& catalog,Integer selected,const WorldMoment& current,ExactArithmeticBudget b){return status([&]{if(selected!=1&&selected!=2)fail("UnsupportedMilitaryOccupationSchema");for(const auto& fact:state.occupation_facts){if(!fact.exact_time)continue;if(selected!=2)fail("UnexpectedMilitaryOccupationExactSource",fact.id);take(read_military_occupation_actual_moment(common,state,catalog,{"military","military_occupation",fact.id},2,current,b));}});}
}
