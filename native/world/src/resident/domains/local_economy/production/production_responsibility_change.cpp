#include "production_responsibility_change.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{v.error};}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::vector<StableId> affected(const EconomicProductionContext&x,const EconomicProductionAdoptionRead&original,const std::vector<PoliticalOriginalResponsibility>&responsibilities){
 std::vector<StableId> units;
 for(const auto&row:x.catalog.production_profiles.rows()){
  bool relevant=false;for(const auto&item:responsibilities){const auto&r=item.responsibility;relevant|=(r.kind==PoliticalResponsibilityKind::production_unit&&r.responsibility_id==row.first)||(r.kind==PoliticalResponsibilityKind::production_asset_use&&r.unit_id&&*r.unit_id==row.first);}
  if(!relevant)continue;const auto scope=take(original.read_original_production_asset_scope(row.first));bool selected=false;
  for(const auto&item:responsibilities){const auto&r=item.responsibility;
   if(r.kind==PoliticalResponsibilityKind::production_unit&&r.responsibility_id==row.first){if(r.unit_id.has_value()||r.asset_id.has_value())fail("ProductionResponsibilityUnitShape",r.responsibility_id);selected=true;}
   if(r.kind==PoliticalResponsibilityKind::production_asset_use&&r.unit_id&&*r.unit_id==row.first){if(!r.asset_id||r.responsibility_id!=*r.asset_id||std::find(scope.production_asset_ids.begin(),scope.production_asset_ids.end(),*r.asset_id)==scope.production_asset_ids.end())fail("ProductionResponsibilityAssetScopeMismatch",r.responsibility_id);selected=true;}
  }
  if(selected)units.push_back(row.first);
 }
 return units;
}
struct ActualChange {const PoliticalTakeoverRegistration*registration=nullptr;WorldMoment at;};
ActualChange actual(const CommonStore&common,const PoliticalTakeoverState&state,const MomentOwnerSource&source){
 if(state.version!=2||source.source_schema!=2||source.source_ref.domain!="politics")fail("ProductionResponsibilityPreciseSourceRequired",source.source_ref.id);
 const auto&ref=source.source_ref;const auto*head=common.state().facts.find(ref.id);if(!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind)fail("ProductionResponsibilitySourceHeaderRequired",ref.id);
 if(ref.kind==political_takeover_pending_fact_kind){const auto*r=state.registrations.find(ref.id);if(!r||r->id!=ref.id||!r->registered_at)fail("ProductionResponsibilityRegistrationRequired",ref.id);return {r,*r->registered_at};}
 if(ref.kind==political_takeover_fact_kind){const auto*a=state.approvals.find(ref.id);const auto*r=a?state.registrations.find(a->registration_fact_id):nullptr;if(!a||!r||a->id!=ref.id||!a->approved_at||a->decision_fact_ids.size()!=r->responsibilities.size())fail("ProductionResponsibilityApprovalRequired",ref.id);for(const auto&id:a->decision_fact_ids){const auto*d=state.decisions.find(id);if(!d||d->approval_fact_id!=a->id)fail("ProductionResponsibilityDecisionCoverage",id);}return {r,*a->approved_at};}
 fail("ProductionResponsibilitySourceKindRequired",ref.id);
}
}
struct ProductionResponsibilityChangeAccess {
 static Outcome<PreparedProductionResponsibilityChange> prepare(const CommonStore&common,const LocalEconomyState&economy,const LocalEconomyCatalog&catalog,const EconomicProductionContext&x,const EconomicProductionAdoptionRead&original,const std::vector<PoliticalOriginalResponsibility>&responsibilities,const WorldMoment&at){try{
  if(compare(at,take(x.clock.read_current_moment())))fail("ProductionResponsibilityActualBoundaryRequired");
  PreparedProductionResponsibilityChange out;out.common_=&common;out.sources_=&x.state;out.at_=at;
  for(const auto&id:affected(x,original,responsibilities)){
   PreparedProductionResponsibilityChange::Entry entry;entry.unit_id=id;const auto*ref=x.state.active_cycles_by_unit.find(id);const auto*cycle=ref?x.state.cycles.find(*ref):nullptr;if(ref&&!cycle)fail("ProductionResponsibilityCycleMissing",id);
   if(cycle&&!cycle->settlement_receipt_ref)entry.close.emplace(take(prepare_production_input_close(common,economy,catalog,x,id,at)));out.entries_.push_back(std::move(entry));
  }out.valid_=true;return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
 static Outcome<std::vector<SourceRef>> apply(CommonStore&common,EconomicProductionContext&x,const EconomicProductionAdoptionRead&original,const PoliticalTakeoverState&state,PreparedProductionResponsibilityChange&&prepared,const MomentOwnerSource&source){try{
  if(!prepared.valid_||prepared.common_!=&common||prepared.sources_!=&x.state)fail("ProductionResponsibilityPreparedCandidateMismatch",source.source_ref.id);prepared.valid_=false;
  const auto change=actual(common,state,source);if(compare(change.at,prepared.at_)||compare(change.at,take(x.clock.read_current_moment())))fail("ProductionResponsibilityChangeBoundaryMismatch",source.source_ref.id);
  const auto expected=affected(x,original,change.registration->responsibilities);if(expected.size()!=prepared.entries_.size())fail("ProductionResponsibilityAffectedUnitsChanged",source.source_ref.id);
  std::vector<SourceRef> results;
  for(std::size_t i=0;i<expected.size();++i){auto&entry=prepared.entries_[i];if(entry.unit_id!=expected[i])fail("ProductionResponsibilityAffectedUnitsChanged",source.source_ref.id);
   // The actual input reader replays the entire true S6 source and decision cut.
   const auto*id=x.state.active_cycles_by_unit.find(entry.unit_id);const auto*cycle=id?x.state.cycles.find(*id):nullptr;if(id&&!cycle)fail("ProductionResponsibilityCycleMissing",entry.unit_id);
   if(!entry.close){if(cycle&&!cycle->settlement_receipt_ref)fail("ProductionResponsibilityCycleOpenedDuringChange",entry.unit_id);continue;}
   auto closed=take(apply_prepared_production_input_close_in_candidate(common,x,std::move(*entry.close),source.source_ref));if(closed)results.push_back({"local_economy","production_input_segment",*closed});
   if(!cycle)fail("ProductionResponsibilityCycleMissing",entry.unit_id);if(compare(change.at,{cycle->due_tick,{}})<0)take(install_changed_production_input_source_in_candidate(x.state,x.inputs,entry.unit_id,change.at,source));
  }
  return {true,std::move(results),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
};
Outcome<PreparedProductionResponsibilityChange> prepare_production_responsibility_change(const CommonStore&common,const LocalEconomyState&economy,const LocalEconomyCatalog&catalog,const EconomicProductionContext&x,const EconomicProductionAdoptionRead&original,const std::vector<PoliticalOriginalResponsibility>&responsibilities,const WorldMoment&at){return ProductionResponsibilityChangeAccess::prepare(common,economy,catalog,x,original,responsibilities,at);}
Outcome<std::vector<SourceRef>> apply_production_responsibility_change_in_candidate(CommonStore&common,EconomicProductionContext&x,const EconomicProductionAdoptionRead&original,const PoliticalTakeoverState&state,PreparedProductionResponsibilityChange&&prepared,const MomentOwnerSource&source){return ProductionResponsibilityChangeAccess::apply(common,x,original,state,std::move(prepared),source);}
}
