// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "economic_facility_source_pins.hpp"
#include <type_traits>

namespace hanmo::resident { namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code){throw Failure{common_failure(code).error};}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> s){if(!s.ok)throw Failure{std::move(s.error)};return std::move(s.value);}
template<class F>Status run(F f){try{f();return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
template<class T>void owned(const std::vector<T>& rows,const T& row){
 for(const auto& actual:rows)if(&actual==&row)return;
 fail("EconomicFacilityPinSourceNotOwned");
}
template<class T>const StableId& owned(const OrderedTable<T>& rows,const T& row){
 for(const auto& actual:rows.rows())if(&actual.second==&row)return actual.first;
 fail("EconomicFacilityPinSourceNotOwned");
}
void selected(const EconomicFacilitySourcesState& state){
 if(state.schema_version!=1)fail("UnsupportedEconomicFacilitySourceSchema");
}
const PhysicalServiceFacilityExtension& extension(const EconomicFacilitySourcesState& state,const StableId& id){
 const PhysicalServiceFacilityExtension* found=nullptr;
 for(const auto& row:state.facilities)if(row.facility_id==id){if(found)fail("DuplicateFacilityIdentity");found=&row;}
 if(!found)fail("EconomicFacilityMissing");
 return *found;
}
void source(EconomicFacilitySourcePinSink& sink,EconomicFacilityPinPath path,const SourceRef& ref,Integer schema){
 if(!valid_source_ref(ref)||schema<=0)fail("InvalidFacilityPinSource");
 take(sink.source(path,ref,schema));
}
void unresolved(EconomicFacilitySourcePinSink& sink,EconomicFacilityPinPath path,const SourceRef& ref){
 if(!valid_source_ref(ref))fail("InvalidFacilityPinSource");
 take(sink.source_requiring_original_schema(path,ref));
}
void rule(EconomicFacilitySourcePinSink& sink,EconomicFacilityPinPath path,const EconomicRuleRef& ref){
 if(ref.id.empty()||ref.version<=0||ref.catalog_digest.empty())fail("InvalidFacilityPinRule");
 take(sink.rule(path,ref));
}
void activation(EconomicFacilitySourcePinSink& sink,EconomicFacilityPinPath path,const FacilityActivationReceiptRef& ref){
 if(ref.receipt_id.empty()||ref.source_schema==0)fail("InvalidFacilityPinActivation");
 take(sink.activation(path,ref));
}
void initialization(EconomicFacilitySourcePinSink& sink,EconomicFacilityPinPath path,const PhysicalServiceFacilityExtension& f){
 if(const auto* installed=std::get_if<InstalledPhysicalFacility>(&f.initialization_basis)){
  if(installed->installation_evidence.installation_id.empty()||installed->installation_evidence.source_schema!=1)fail("InvalidFacilityPinInstallation");
  take(sink.installation(path,f));
 }else activation(sink,path,std::get<ActivatedPhysicalFacility>(f.initialization_basis).original_facility_activation_receipt_ref);
}
// 比对已保存与实际设施的初始化凭据；类型或来源不一致时拒绝。
void initialization_address(const PhysicalFacilityInitialization &saved,
                            const PhysicalServiceFacilityExtension &f) {
  if (saved.index() != f.initialization_basis.index())
    fail("FacilityPinInitializationMismatch");
  if (const auto *a = std::get_if<InstalledPhysicalFacility>(&saved)) {
    const auto &b = std::get<InstalledPhysicalFacility>(f.initialization_basis)
                        .installation_evidence;
    if (a->installation_evidence.installation_id != b.installation_id ||
        a->installation_evidence.source_schema != b.source_schema)
      fail("FacilityPinInitializationMismatch");
  } else {
    const auto &saved_activation =
        std::get<ActivatedPhysicalFacility>(saved)
            .original_facility_activation_receipt_ref;
    const auto &b = std::get<ActivatedPhysicalFacility>(f.initialization_basis)
                        .original_facility_activation_receipt_ref;
    if (saved_activation.receipt_id != b.receipt_id ||
        saved_activation.source_schema != b.source_schema)
      fail("FacilityPinInitializationMismatch");
  }
}
void base(EconomicFacilitySourcePinSink& sink,const EconomicFacilityBaseBinding& value){
 std::visit([&](const auto& v){
  using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,OriginalConditionBaseSlot>){
   if(v.condition_ref.empty())fail("InvalidFacilityPinCondition");
   take(sink.condition_seed({"base.condition_ref"},v.condition_ref));
   rule(sink,{"base.original_slot_definition_ref"},v.original_slot_definition_ref);
  }else if constexpr(std::is_same_v<T,InstalledServiceUnits>)rule(sink,{"base.installed_definition_ref"},v.installed_definition_ref);
  else activation(sink,{"base.activation_receipt_ref"},v.activation_receipt_ref);
 },value);
}
void claim(EconomicFacilitySourcePinSink& sink,const EconomicFacilitySourcesState& state,EconomicFacilityPinPath path,const StableId& id){
 if(id.empty())fail("InvalidFacilityPinClaim");
 take(sink.claim(path,id));
 const FacilityPhysicalClaimBinding* found=nullptr;
 for(const auto& b:state.physical_bindings)if(b.claim_id==id){if(found)fail("DuplicatePhysicalClaimBinding");found=&b;}
 // Aggregate claims genuinely have no physical binding. Whether a missing row
 // is legal for this claim is checked by the original complete holder gate.
 if(found)take(sink.physical_binding(path,*found));
}
const ProductionFacilityConditionSeed& condition_seed(const ProductionFacilityConditionsCatalog& catalog,const StableId& asset){
 const ProductionFacilityConditionSeed* found=nullptr;
 for(const auto& row:catalog.seeds.rows())if(row.second.original_asset_ref==asset){
  if(found||row.first!=row.second.condition_ref)fail("DuplicateFacilityConditionAsset");
  found=&row.second;
 }
 if(!found)fail("FacilityConditionSourceMissing");
 return *found;
}
} // namespace

Status visit_economic_facility_definition_pins(const EconomicFacilitySourcesState& state,const EconomicFacilitySlotDefinition& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.definitions,row);
 rule(sink,{"definition_ref"},row.definition_ref);base(sink,row.base_binding);
 rule(sink,{"availability_policy_ref"},row.availability_policy_ref);
 rule(sink,{"usage_policy_ref"},row.usage_policy_ref);
 std::visit([&](const auto& layout){
  using T=std::decay_t<decltype(layout)>;
  if constexpr(std::is_same_v<T,AggregateFacilityLayout>)rule(sink,{"physical_layout.original_layout_rule_ref"},layout.original_layout_rule_ref);
  else {
   rule(sink,{"physical_layout.layout_definition_ref"},layout.layout_definition_ref);
   if constexpr(std::is_same_v<T,IndependentFacilityUnits>)rule(sink,{"physical_layout.independence_policy_ref"},layout.independence_policy_ref);
   else rule(sink,{"physical_layout.whole_zone_policy_ref"},layout.whole_zone_policy_ref);
   for(std::size_t i=0;i<layout.unit_definitions.size();++i)
    for(std::size_t j=0;j<layout.unit_definitions[i].allowed_purpose_refs.size();++j)
     rule(sink,{"physical_layout.allowed_purpose_refs",i,j},layout.unit_definitions[i].allowed_purpose_refs[j]);
  }
 },row.physical_layout);
});}
Status visit_economic_facility_extension_pins(const EconomicFacilitySourcesState& state,const PhysicalServiceFacilityExtension& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.facilities,row);
 rule(sink,{"definition_ref"},row.definition_ref);initialization(sink,{"initialization_basis"},row);
});}
Status visit_economic_facility_installation_pins(const EconomicFacilitySourcesState& state,const PhysicalServiceFacilityExtension& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.facilities,row);
 const auto* installed=std::get_if<InstalledPhysicalFacility>(&row.initialization_basis);
 if(!installed||installed->installation_evidence.source_schema!=1||installed->installation_evidence.installation_id.empty())fail("InvalidFacilityPinInstallation");
 const auto& v=installed->installation_evidence;
 // The inline child's actual parent is explicit; no collector address-parent
 // inference or recursive visitor invocation is relied upon.
 take(sink.extension({"original_extension"},row));
 take(sink.catalog({"catalog_identity"},v.catalog_identity));
 take(sink.seed({"seed_row_ref"},v.seed_row_ref,v.catalog_identity));
 rule(sink,{"definition_ref"},v.definition_ref);
});}
Status visit_economic_facility_capacity_pins(const EconomicFacilitySourcesState& state,const EconomicFacilityCapacitySource& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.capacity_sources,row);
 if(row.source_schema!=1||row.source_id.empty()||row.source_revision<=0)fail("InvalidFacilityPinCapacity");
 const auto& f=extension(state,row.facility_extension_ref);
 if(f.facility_id!=row.slot_ref.facility_id||f.asset_owner_domain!=row.slot_ref.provider_domain)fail("FacilityPinExtensionMismatch");
 take(sink.extension({"facility_extension_ref"},f));
 rule(sink,{"definition_ref"},row.definition_ref);
 if(row.previous_source_ref)source(sink,{"previous_source_ref"},*row.previous_source_ref,1);
 base(sink,row.base_basis);
 std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,FacilityNotConditioned>)rule(sink,{"condition.original_asset_definition_ref"},v.original_asset_definition_ref);
  else {
   if(v.condition_ref.empty()||v.condition_revision<=0)fail("InvalidFacilityPinCondition");
   take(sink.condition_revision({"condition.condition_ref"},v.condition_ref,v.condition_revision));
   source(sink,{"condition.condition_change_source_ref"},v.condition_change_source_ref,1);
  }
 },row.condition_basis);
 std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,OriginalFacilityOpenRule>){
   rule(sink,{"availability.rule_ref"},v.rule_ref);
   if(v.original_acceptance_ref)unresolved(sink,{"availability.original_acceptance_ref"},*v.original_acceptance_ref);
  }else unresolved(sink,{"availability.original_opening_source_ref"},v.original_opening_source_ref);
 },row.availability_basis);
 source(sink,{"usage.original_right_ref"},row.usage_right_basis.original_right_ref,row.usage_right_basis.original_right_schema);
 rule(sink,{"usage.usage_policy_ref"},row.usage_right_basis.usage_policy_ref);
 if(row.usage_right_basis.original_acceptance_ref)unresolved(sink,{"usage.original_acceptance_ref"},*row.usage_right_basis.original_acceptance_ref);
 std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(!std::is_same_v<T,AdmitWithinCapacity>){
   for(std::size_t i=0;i<v.retention_source_refs.size();++i)source(sink,{"admission.retention_source_refs",i},v.retention_source_refs[i].source_ref,v.retention_source_refs[i].source_schema);
   if constexpr(std::is_same_v<T,EntireSlotNoNewAdmission>)source(sink,{"admission.whole_slot_restriction_source_ref"},v.whole_slot_restriction_source_ref,v.whole_slot_restriction_source_schema);
  }
 },row.admission_mode);
 std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,FacilityInitializedCause>){initialization_address(v.initialization_basis,f);initialization(sink,{"change.initialization_basis"},f);}
  else if constexpr(std::is_same_v<T,FacilityActivatedCause>)activation(sink,{"change.activation_receipt_ref"},v.activation_receipt_ref);
  else if constexpr(std::is_same_v<T,FacilityConditionChangedCause>)source(sink,{"change.condition_change_ref"},v.condition_change_ref,1);
  else if constexpr(std::is_same_v<T,FacilityOpeningChangedCause>)source(sink,{"change.opening_result_ref"},v.opening_result_ref.source_ref,v.opening_result_ref.source_schema);
  else if constexpr(std::is_same_v<T,FacilityRightChangedCause>)source(sink,{"change.right_result_ref"},v.right_result_ref.source_ref,v.right_result_ref.source_schema);
  else source(sink,{"change.retention_transition_ref"},v.retention_transition_ref,1);
 },row.change_cause);
});}
Status visit_economic_facility_binding_pins(const EconomicFacilitySourcesState& state,const FacilityPhysicalClaimBinding& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.physical_bindings,row);
 if(row.claim_id.empty())fail("InvalidFacilityPinClaim");
 take(sink.claim({"claim_id"},row.claim_id));
 take(sink.extension({"slot_ref"},extension(state,row.slot_ref.facility_id)));
 rule(sink,{"definition_ref"},row.definition_ref);
 source(sink,{"admission_business_source_ref"},row.admission_business_source_ref,row.admission_source_schema);
 for(std::size_t i=0;i<row.physical_unit_refs.size();++i)std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,InstalledPhysicalUnitSource>)source(sink,{"physical_unit_refs.installation_source_ref",i},v.source_ref,v.source_schema);
  else activation(sink,{"physical_unit_refs.activation_receipt_ref",i},v.original_facility_activation_receipt_ref);
 },row.physical_unit_refs[i].installation_source_ref);
});}
Status visit_economic_facility_retention_pins(const EconomicFacilitySourcesState& state,const FacilityRetentionTransition& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(state);owned(state.retention_transitions,row);
 if(row.source_schema!=1||row.transition_id.empty())fail("InvalidFacilityPinRetention");
 take(sink.extension({"slot_ref"},extension(state,row.slot_ref.facility_id)));
 source(sink,{"cause_ref"},row.cause_ref,row.cause_schema);
 for(std::size_t i=0;i<row.entries.size();++i)std::visit([&](const auto& v){
  using T=std::decay_t<decltype(v)>;
  claim(sink,state,{"entries.origin_claim_id",i},v.origin_claim_id);
  claim(sink,state,{"entries.previous_claim_id",i},v.previous_claim_id);
  if constexpr(std::is_same_v<T,FacilityRetentionBegin>)take(sink.claim_receipt({"entries.origin_admission_receipt_ref",i},v.origin_admission_receipt_ref));
  else source(sink,{"entries.previous_retention_source_ref",i},v.previous_retention_source_ref,1);
  if constexpr(std::is_same_v<T,FacilityRetentionBegin>||std::is_same_v<T,FacilityRetentionContinue>){
   claim(sink,state,{"entries.retained_claim_id",i},v.retained_claim_id);
   source(sink,{"entries.holder_occupancy_source_ref",i},v.holder_occupancy_source_ref,v.holder_source_schema);
   if constexpr(std::is_same_v<T,FacilityRetentionContinue>)if(v.clearance_source_ref)unresolved(sink,{"entries.clearance_source_ref",i},*v.clearance_source_ref);
  }else if constexpr(std::is_same_v<T,FacilityRetentionClear>)source(sink,{"entries.holder_clearance_source_ref",i},v.holder_clearance_source_ref,v.holder_source_schema);
  else {
   claim(sink,state,{"entries.normal_claim_id",i},v.normal_claim_id);
   source(sink,{"entries.actual_resume_result_ref",i},v.actual_resume_result_ref,v.actual_resume_source_schema);
   source(sink,{"entries.holder_occupancy_source_ref",i},v.holder_occupancy_source_ref,v.holder_source_schema);
  }
 },row.entries[i]);
 for(std::size_t i=0;i<row.normal_disposition_result_refs.size();++i)source(sink,{"normal_disposition_result_refs",i},row.normal_disposition_result_refs[i].source_ref,row.normal_disposition_result_refs[i].source_schema);
});}
Status visit_production_facility_condition_pins(const ProductionFacilityConditionsState& state,const ProductionFacilityConditionsCatalog& catalog,const EconomicFacilitySourcesState& facilities,const ProductionFacilityCondition& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(facilities);const auto& key=owned(state.conditions,row);
 const auto& seed=condition_seed(catalog,row.facility_ref);
 if(seed.condition_ref!=key||row.condition_revision<=0)fail("FacilityConditionHeadMismatch");
 const auto* f=take(read_production_facility_condition_installation(seed,facilities));
 take(sink.condition_seed({"condition_seed"},seed.condition_ref));
 take(sink.installation({"original_installation_seed_ref"},*f));
 rule(sink,{"damage_policy_ref"},row.damage_policy_ref);rule(sink,{"repair_policy_ref"},row.repair_policy_ref);
 source(sink,{"last_change_ref"},row.last_change_ref,1);
});}
Status visit_production_facility_condition_change_pins(const CommonStore& common,const ProductionFacilityConditionsState& state,const ProductionFacilityConditionsCatalog& catalog,const EconomicFacilitySourcesState& facilities,const FacilityConditionChange& row,EconomicFacilitySourcePinSink& sink){return run([&]{
 selected(facilities);const auto& key=owned(state.changes,row);
 const auto* head=common.state().facts.find(key);
 if(!head)fail("FacilityConditionOriginalHeaderMissing");
 take(read_production_facility_condition_fact_payload_schema(common,state,facilities,*head));
 const auto& seed=condition_seed(catalog,row.facility_ref);
 const auto* f=take(read_production_facility_condition_installation(seed,facilities));
 take(sink.condition_seed({"condition_seed"},seed.condition_ref));
 take(sink.installation({"original_installation_seed_ref"},*f));
 if(row.old_revision<=0)fail("InvalidFacilityPinCondition");
 take(sink.condition_revision({"old_revision"},seed.condition_ref,row.old_revision));
 std::visit([&](const auto& v){source(sink,{"source"},v.source,v.source_schema);},row.source);
 for(std::size_t i=0;i<row.interrupted_activity_results.size();++i)source(sink,{"interrupted_activity_results",i},row.interrupted_activity_results[i].source_ref,row.interrupted_activity_results[i].source_schema);
 for(std::size_t i=0;i<row.claim_release_results.size();++i)take(sink.claim_receipt({"claim_release_results",i},row.claim_release_results[i]));
});}
}
