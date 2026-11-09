#include "facility_condition_mutation.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}) {throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value) {if(!value.ok)throw Failure{value.error};return std::move(value.value);}
void take(Status value) {if(!value.ok)throw Failure{value.error};}
int compare(const WorldMoment&a,const WorldMoment&b) {return take(compare_world_moments(a,b));}
bool same(const FacilitySourceRef&a,const FacilitySourceRef&b) {return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
const PhysicalServiceFacilityExtension& facility(const EconomicFacilityProviderContext&x,const FacilitySlotRef&slot) {
 const PhysicalServiceFacilityExtension* found=nullptr;
 for(const auto&row:x.state.facilities)if(row.facility_id==slot.facility_id) {if(found)fail("DuplicateFacilityInstallation",slot.facility_id);found=&row;}
 if(!found||found->asset_owner_domain!=slot.provider_domain)fail("FacilityConditionSlotOwnerMismatch",slot.facility_id);
 return *found;
}
std::vector<FacilitySlotRef> asset_slots(const EconomicFacilityProviderContext&x,const StableId&asset) {
 std::vector<FacilitySlotRef> out;
 for(const auto&f:x.state.facilities)if(f.original_asset_ref==asset) {
  bool found=false;
  for(const auto&s:x.state.capacity_sources)if(s.facility_extension_ref==f.facility_id) {
   if(s.slot_ref.facility_id!=f.facility_id||s.slot_ref.provider_domain!=f.asset_owner_domain)fail("FacilityConditionSlotOwnerMismatch",f.facility_id);
   if(std::find(out.begin(),out.end(),s.slot_ref)==out.end())out.push_back(s.slot_ref);
   found=true;
  }
  if(!found)fail("FacilityOriginalCapacitySourceRequired",f.facility_id);
 }
 if(out.empty())fail("FacilityConditionOriginalInstallationRequired",asset);
 return out;
}
std::size_t source_order(const CommonStore&common,const SourceRef&source) {
 const auto*head=common.state().facts.find(source.id);const auto&order=common.state().facts.order();
 const auto position=std::find(order.begin(),order.end(),source.id);
 if(!head||head->domain!=source.domain||head->kind!=source.kind||position==order.end())fail("FacilityConditionSourceOrderRequired",source.id);
 return static_cast<std::size_t>(position-order.begin());
}
std::vector<FacilitySlotRef> slots_at_change(const EconomicFacilityProviderContext&x,const FacilityConditionChange&change,const SourceRef&cut) {
 auto all=asset_slots(x,change.facility_ref);std::vector<FacilitySlotRef> out;
 for(const auto&slot:all) {
  const auto&f=facility(x,slot);bool installed=false;
  if(const auto*initial=std::get_if<InstalledPhysicalFacility>(&f.initialization_basis))installed=compare(initial->installation_evidence.installed_at,change.occurred_at)<=0;
  else {
   const auto original=take(x.applied.read_applied_facility_activation(std::get<ActivatedPhysicalFacility>(f.initialization_basis).original_facility_activation_receipt_ref));
   const auto time=compare(original.occurred_at,change.occurred_at);
   installed=time<0||(time==0&&source_order(x.common,original.original_source)<source_order(x.common,cut));
  }
  if(installed)out.push_back(slot);
 }
 if(out.empty())fail("FacilityConditionOriginalInstallationRequired",change.facility_ref);
 return out;
}
void binding(const NativeProductionFacilityMutationContext&x,const WorldMoment&at) {
 const auto&r=x.reconciliation.read;
 if(&x.common!=&r.common||&x.facilities!=&r.state||&x.claims!=&r.claims||&x.reconciliation.conditions!=&r.conditions||
    compare(r.current,at)||compare(take(x.production.clock.read_current_moment()),at))fail("FacilityMutationCandidateMismatch");
}
const FacilityConditionChange& change(const EconomicFacilityProviderContext&x,const FacilityCapacityReconciliationRequest&request) {
 const auto&ref=request.original_change;
 if(ref.source_schema!=1||ref.source_ref.kind!="facility_condition_change"||ref.source_ref.domain!=request.slot_ref.provider_domain)fail("FacilityConditionActualSourceRequired",ref.source_ref.id);
 const auto*row=x.conditions.changes.find(ref.source_ref.id);
 const auto*head=x.common.state().facts.find(ref.source_ref.id);
 if(!row||row->change_id!=ref.source_ref.id||!head||head->id!=ref.source_ref.id||head->domain!=ref.source_ref.domain||head->kind!=ref.source_ref.kind||
    row->facility_ref!=facility(x,request.slot_ref).original_asset_ref||row->old_revision==row->new_revision||compare(row->occurred_at,request.occurred_at)||compare(row->occurred_at,x.current))fail("FacilityConditionActualSourceMismatch",ref.source_ref.id);
 const auto slots=asset_slots(x,row->facility_ref);
 if(std::find(slots.begin(),slots.end(),request.slot_ref)==slots.end())fail("FacilityConditionOriginalSlotRequired",request.slot_ref.facility_id);
 return *row;
}
const EconomicFacilityCapacitySource* capacity_change(const EconomicFacilityProviderContext&x,const FacilitySlotRef&slot,const SourceRef&ref) {
 const EconomicFacilityCapacitySource* found=nullptr;
 for(const auto&s:x.state.capacity_sources)if(s.slot_ref==slot)if(const auto*c=std::get_if<FacilityConditionChangedCause>(&s.change_cause))if(same_source_ref(c->condition_change_ref,ref)) {
  if(found)fail("DuplicateFacilityConditionCapacityAdoption",ref.id);found=&s;
 }
 return found;
}
SourceRef original_effect(const FacilityConditionEffectSource&source) {return std::visit([](const auto&v){return v.source;},source);}
}
Outcome<StableId> apply_native_production_facility_effect_in_candidate(
 NativeProductionFacilityMutationContext&x,const ProductionFacilityEffectRead&effects,const StableId&id,Integer expected,const FacilityConditionEffectSource&source) {
 NativeProductionFacilityMutationServices services(x);
 return apply_production_facility_effect_in_candidate(x.common,x.reconciliation.conditions,x.reconciliation.read.condition_catalog,x.facilities,effects,services,id,expected,source,x.reconciliation.read.arithmetic);
}
Status validate_native_facility_condition_reconciliation(const NativeProductionFacilityMutationContext&x,const FacilityCapacityReconciliationRequest&request) {try {
 binding(x,request.occurred_at);change(x.reconciliation.read,request);
 if(capacity_change(x.reconciliation.read,request.slot_ref,request.original_change.source_ref))fail("FacilityConditionCapacityAlreadyAdopted",request.original_change.source_ref.id);
 NativeEconomicFacilitySourceRead provider(x.reconciliation.read);
 const auto prior=take(provider.capacity_source_at(request.slot_ref,request.occurred_at));
 take(provider.read_facility_slot_definition_at_source(prior,request.slot_ref));
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Outcome<std::optional<FacilityClaimMutationReceipt>> finalize_native_facility_condition_reconciliation_in_candidate(
 NativeProductionFacilityMutationContext&x,const FacilityCapacityReconciliationRequest&request,const std::vector<FacilityClaimMutationReceipt>&results) {try {
 take(validate_native_facility_condition_reconciliation(x,request));
 bool live=false;for(const auto&claim:x.claims.claims)if(claim.slot_ref==request.slot_ref)live=true;
 if(!live) {
  if(!results.empty())fail("FacilityConditionUnexpectedOwnerResults",request.original_change.source_ref.id);
  FacilityAdmissionDisposition disposition=AdmitWithinCapacity{};
  const auto restriction=take(x.reconciliation.read.applied.read_applied_whole_slot_restriction(request.original_change,request.slot_ref));
  if(restriction) {
   if(restriction->source_schema<=0||!valid_source_ref(restriction->source_ref))fail("FacilityConditionRestrictionSourceRequired",request.original_change.source_ref.id);
   disposition=EntireSlotNoNewAdmission{{},restriction->source_ref,restriction->source_schema};
  }
  take(record_economic_facility_capacity_in_candidate(x.common,x.facilities,x.reconciliation.read,request.slot_ref,FacilityConditionChangedCause{request.original_change.source_ref},disposition));
  return {true,std::nullopt,{}};
 }
 if(results.empty())fail("FacilityConditionOwnerResultsRequired",request.original_change.source_ref.id);
 auto receipt=take(finalize_economic_facility_reconciliation_in_candidate(x.common,x.facilities,x.reconciliation,request.slot_ref,request.original_change,results));
 return {true,std::move(receipt),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
Status NativeProductionFacilityMutationServices::close_facility_production_inputs(const StableId&asset,const WorldMoment&at,const SourceRef&effect) {try {
 if(phase_!=Phase::idle||!valid_source_ref(effect))fail("FacilityMutationPreparationOrder",asset);
 binding(x_,at);auto slots=asset_slots(x_.reconciliation.read,asset);
 std::vector<PreparedUnit> prepared;
 for(const auto&row:x_.production.catalog.production_profiles.rows()) {
  // Filter by the original locked asset plan before asking for full unit proof.
  const auto*management=x_.economy_catalog.production_management?&*x_.economy_catalog.production_management:nullptr;
  const auto*plan=management?management->plans.find(row.first):nullptr;
  if(!plan||std::none_of(plan->asset_uses.begin(),plan->asset_uses.end(),[&](const auto&use){return use.asset_id==asset;}))continue;
  const auto scope=take(x_.adoptions.read_original_production_asset_scope(row.first));
  if(std::find(scope.production_asset_ids.begin(),scope.production_asset_ids.end(),asset)==scope.production_asset_ids.end())fail("FacilityMutationOriginalAssetScopeMismatch",row.first);
  PreparedUnit entry;entry.unit_id=row.first;
  const auto*id=x_.production.state.active_cycles_by_unit.find(row.first);const auto*cycle=id?x_.production.state.cycles.find(*id):nullptr;
  if(id&&!cycle)fail("FacilityMutationProductionCycleMissing",row.first);
  if(cycle&&!cycle->settlement_receipt_ref)entry.close.emplace(take(prepare_production_input_close(x_.common,x_.economy,x_.economy_catalog,x_.production,row.first,at,x_.reconciliation.read.arithmetic)));
  prepared.push_back(std::move(entry));
 }
 asset_id_=asset;effect_source_=effect;at_=at;slots_=std::move(slots);units_=std::move(prepared);phase_=Phase::prepared;
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Status NativeProductionFacilityMutationServices::reconcile_facility_capacity_change(FacilityConditionChange&actual) {try {
 if(phase_!=Phase::prepared||actual.facility_ref!=asset_id_||!same_source_ref(original_effect(actual.source),effect_source_)||compare(actual.occurred_at,at_)||
    x_.reconciliation.conditions.changes.find(actual.change_id)!=&actual||actual.old_revision==actual.new_revision)fail("FacilityMutationActualChangeMismatch",actual.change_id);
 binding(x_,at_);const auto current_slots=asset_slots(x_.reconciliation.read,asset_id_);
 if(current_slots!=slots_)fail("FacilityMutationOriginalSlotsChanged",actual.change_id);
 const auto*head=x_.common.state().facts.find(actual.change_id);
 if(!head||head->id!=actual.change_id||head->kind!="facility_condition_change")fail("FacilityMutationActualHeaderRequired",actual.change_id);
 condition_source_={head->domain,head->kind,head->id};
 for(auto&entry:units_) {
  const auto*id=x_.production.state.active_cycles_by_unit.find(entry.unit_id);const auto*cycle=id?x_.production.state.cycles.find(*id):nullptr;
  if(id&&!cycle)fail("FacilityMutationProductionCycleMissing",entry.unit_id);
  if(entry.close)take(apply_prepared_production_input_close_in_candidate(x_.common,x_.production,std::move(*entry.close),condition_source_,x_.reconciliation.read.arithmetic));
  else if(cycle&&!cycle->settlement_receipt_ref)fail("FacilityMutationCycleOpenedDuringChange",entry.unit_id);
 }
 NativeEconomicFacilitySourceRead sources(x_.reconciliation.read);
 for(const auto&slot:slots_) {
  FacilityCapacityReconciliationRequest request{slot,{condition_source_,1},at_};
  FacilityCapacityReconciliationRegistration registration;
  registration.provider_domain=slot.provider_domain;registration.profile_id="production_facility_condition";registration.profile_version=1;
  registration.validate_request=[this](const WorldBundle&world,const FacilityCapacityReconciliationRequest&r) {
   if(&world!=&x_.world)return common_failure("FacilityMutationWorldMismatch");
   return validate_native_facility_condition_reconciliation(x_,r);
  };
  registration.finalize=[this](WorldBundle&world,const FacilityCapacityReconciliationRequest&r,const std::vector<FacilityClaimMutationReceipt>&owners,ExactPhaseWriteContext&writes)->Outcome<std::optional<FacilityClaimMutationReceipt>> {
   if(&world!=&x_.world||&writes!=&x_.phase_writes)return {false,{},common_failure("FacilityMutationWorldMismatch").error};
   return finalize_native_facility_condition_reconciliation_in_candidate(x_,r,owners);
  };
  take(reconcile_facility_capacity_in_candidate(x_.world,x_.claims,x_.claim_catalog,request,registration,x_.holder_handlers,x_.reconciliation.read.holders,sources,x_.phase_writes,x_.reconciliation.read.arithmetic));
 }
 phase_=Phase::reconciled;return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Status NativeProductionFacilityMutationServices::reopen_facility_production_inputs(const StableId&asset,const WorldMoment&at) {try {
 if(phase_!=Phase::reconciled||asset!=asset_id_||compare(at,at_))fail("FacilityMutationReopenOrder",asset);
 binding(x_,at);
 for(const auto&entry:units_)if(entry.close) {
  const auto*id=x_.production.state.active_cycles_by_unit.find(entry.unit_id);const auto*cycle=id?x_.production.state.cycles.find(*id):nullptr;
  if(!cycle||cycle->settlement_receipt_ref)fail("FacilityMutationProductionCycleChanged",entry.unit_id);
  if(compare(at,{cycle->due_tick,{}})<0)take(install_changed_production_input_source_in_candidate(x_.production.state,x_.production.inputs,entry.unit_id,at,{condition_source_,1}));
 }
 phase_=Phase::idle;units_.clear();slots_.clear();asset_id_.clear();effect_source_={};condition_source_={};return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Status validate_production_facility_capacity_adoptions(const EconomicFacilityProviderContext&x) {try {
 NativeEconomicFacilitySourceRead provider(x);
 for(const auto&row:x.conditions.changes.rows()) {
  const auto&actual=row.second;
  const auto*head=x.common.state().facts.find(row.first);
  if(!head||head->id!=row.first||head->kind!="facility_condition_change"||actual.change_id!=row.first)fail("FacilityConditionActualHeaderRequired",row.first);
  const SourceRef original{head->domain,head->kind,head->id};
  const auto slots=slots_at_change(x,actual,original);
  std::vector<FacilityClaimReceiptRef> receipts;
  for(const auto&slot:slots) {
   const auto*capacity=capacity_change(x,slot,original);
   if(actual.old_revision==actual.new_revision) {if(capacity)fail("FacilityUnchangedConditionHasCapacityAdoption",row.first);continue;}
   if(!capacity||compare(capacity->occurred_at,actual.occurred_at))fail("FacilityConditionCapacityAdoptionMissing",row.first);
   take(provider.validate_source(*capacity));
   const FacilitySourceRef capacity_ref{{slot.provider_domain,"economic_facility_capacity",capacity->source_id},1};
   bool reconciled=false;
   for(const auto&history:x.claims.slot_histories)if(history.slot_ref==slot)for(const auto&receipt:history.mutations) {
    const auto order=take(provider.compare_mutation_to_capacity_change(slot,receipt,FacilityCapacityChange{capacity_ref,capacity->occurred_at,capacity->source_revision,capacity->previous_source_ref?std::optional<FacilitySourceRef>{FacilitySourceRef{*capacity->previous_source_ref,1}}:std::nullopt}));
    if(order!=FacilityMutationCapacityOrder::reconciles)continue;
    if(reconciled||compare(receipt.occurred_at,actual.occurred_at)||std::none_of(receipt.capacity_source_refs.begin(),receipt.capacity_source_refs.end(),[&](const auto&ref){return same(ref,capacity_ref);}))fail("FacilityConditionClaimAdoptionMismatch",row.first);
    reconciled=true;take(provider.validate_mutation_source(slot,receipt));receipts.push_back({slot,receipt.claim_set_revision});
   }
  }
  if(actual.old_revision==actual.new_revision&&!actual.interrupted_activity_results.empty())fail("FacilityUnchangedConditionHasDispositions",row.first);
  if(receipts.size()!=actual.claim_release_results.size())fail("FacilityConditionClaimReceiptCoverage",row.first);
  for(const auto&ref:receipts)if(std::count_if(actual.claim_release_results.begin(),actual.claim_release_results.end(),[&](const auto&stored){return stored.slot_ref==ref.slot_ref&&stored.claim_set_revision==ref.claim_set_revision;})!=1)fail("FacilityConditionClaimReceiptCoverage",row.first);
 }
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
}
