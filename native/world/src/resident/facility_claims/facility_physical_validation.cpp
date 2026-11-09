// 原物理source reader先核真实WIP/清场，再由此处独立反查S0 admission/close和所有竞争者。
#include "facility_physical_validation.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
namespace hanmo::resident {
namespace {
struct Failure { RuleError error; };
void fail(const char* code){RuleError e;e.original={code,"facility_physical_claims"};throw Failure{std::move(e)};}
template<class T> T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
template<class T,class F> Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"FacilityPhysicalResourceExhausted","facility_claims"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"FacilityPhysicalResourceExhausted","facility_claims"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
bool same(const WorldMoment& a,const WorldMoment& b,ExactArithmeticBudget x){return take(compare_world_moments(a,b,x))==0;}
bool covers(const MomentSpan& outer,const MomentSpan& inner,ExactArithmeticBudget b){return take(compare_world_moments(outer.from,inner.from,b))<=0&&take(compare_world_moments(inner.to,outer.to,b))<=0;}
Integer sum(Integer a,Integer n){if(n<0||n>std::numeric_limits<Integer>::max()-a)fail("FacilityPhysicalUnitsOverflow");return a+n;}
const std::vector<FacilitySourceRef>* retention_refs(const FacilityCapacitySource2& source){if(auto* d=std::get_if<NormalWithRetentions>(&source.admission_disposition))return &d->retention_source_refs;if(auto* d=std::get_if<EntireSlotNoNewAdmission>(&source.admission_disposition))return &d->retention_source_refs;return nullptr;}
// 原binding必须是同claim实际采用结果；安装/allowed-purpose等完整真实源由原reader先验。
FacilityPhysicalBindingProof binding(const FacilityClaimsState& state,const FacilityClaim2& claim,const WorldMoment& current,const EconomicFacilityOriginalSourceRead& reader,ExactArithmeticBudget b){auto proof=take(reader.read_physical_binding_proof(claim.claim_id));const auto& p=proof.binding;auto episode=take(read_facility_claim_episode(state,claim.claim_id,current,b));auto admitted=take(read_facility_claim_receipt(state,episode.admission_receipt_ref));if(p.claim_id!=claim.claim_id||p.slot_ref!=claim.slot_ref||!same_source_ref(p.admission_business_source_ref,claim.source_result_ref)||p.admission_source_schema!=admitted.source_schema||!same(p.occurred_at,admitted.occurred_at,b)||p.definition_ref.id!=proof.definition.definition_ref.id||p.definition_ref.version!=proof.definition.definition_ref.version||p.definition_ref.catalog_digest!=proof.definition.definition_ref.catalog_digest||proof.facility.facility_id!=claim.slot_ref.facility_id)fail("FacilityPhysicalAdmissionMismatch");if(p.physical_unit_refs.size()!=static_cast<std::size_t>(claim.units))fail("FacilityPhysicalUnitCountMismatch");std::set<StableId> units;for(const auto& unit:p.physical_unit_refs)if(unit.facility_id!=claim.slot_ref.facility_id||unit.unit_key.empty()||!units.insert(unit.unit_key).second)fail("DuplicateFacilityPhysicalUnit");return proof;}
// physical unit的安装来源是身份的一部分，同名unit不能跨安装/activation冒原位。
bool same_unit(const FacilityPhysicalUnitRef& a,const FacilityPhysicalUnitRef& z){
    if(a.facility_id!=z.facility_id||a.unit_key!=z.unit_key||a.installation_source_ref.index()!=z.installation_source_ref.index())return false;
    if(const auto* x=std::get_if<InstalledPhysicalUnitSource>(&a.installation_source_ref)){const auto& y=std::get<InstalledPhysicalUnitSource>(z.installation_source_ref);return x->source_schema==y.source_schema&&same_source_ref(x->source_ref,y.source_ref);}
    const auto& x=std::get<ActivatedPhysicalUnitSource>(a.installation_source_ref).original_facility_activation_receipt_ref;
    const auto& y=std::get<ActivatedPhysicalUnitSource>(z.installation_source_ref).original_facility_activation_receipt_ref;
    return x.receipt_id==y.receipt_id&&x.source_schema==y.source_schema;
}
void retained_positions(const FacilityClaimsState& state,const FacilityClaim2& claim,const StableId& previous_id,const StableId& origin_id,bool may_reduce,const WorldMoment& current,const FacilityCapacitySource2& capacity,const EconomicFacilityOriginalSourceRead& reader,ExactArithmeticBudget budget){
    const auto definition=take(reader.read_facility_slot_definition_at_source({capacity.source_fact_ref,1},claim.slot_ref));if(std::holds_alternative<AggregateFacilityLayout>(definition.physical_layout))return;
    const auto current_binding=binding(state,claim,current,reader,budget);
    const auto previous=take(read_facility_claim_episode(state,previous_id,current,budget));
    const auto origin=take(read_facility_claim_episode(state,origin_id,current,budget));
    const auto before=binding(state,previous.original_claim,current,reader,budget);
    const auto first=binding(state,origin.original_claim,current,reader,budget);
    if(!may_reduce&&current_binding.binding.physical_unit_refs.size()!=before.binding.physical_unit_refs.size())fail("FacilityRetentionChangedPositions");
    for(const auto& unit:current_binding.binding.physical_unit_refs){
        const auto contains=[&](const auto& list){return std::any_of(list.begin(),list.end(),[&](const auto& prior){return same_unit(unit,prior);});};
        if(!contains(before.binding.physical_unit_refs)||!contains(first.binding.physical_unit_refs))fail("FacilityRetentionChangedPositions");
    }
}
// 保位前驱须真的在本mutation关闭，不能只有owner写了一个退位声明。
void predecessor(const FacilityClaimsState& state,const FacilityClaim2& retained,const FacilityRetentionTransition& transition,const StableId& previous_id,const StableId& origin_id,bool reduced,const WorldMoment& current,ExactArithmeticBudget b){auto previous=take(read_facility_claim_episode(state,previous_id,current,b));auto origin=take(read_facility_claim_episode(state,origin_id,current,b));if(!previous.close_receipt_ref||previous.original_claim.holder!=retained.holder||origin.original_claim.holder!=retained.holder||previous.original_claim.slot_ref!=retained.slot_ref||origin.original_claim.slot_ref!=retained.slot_ref||retained.units>previous.original_claim.units||(retained.units<previous.original_claim.units&&!reduced)||!same_source_ref(retained.usage_right_ref,origin.original_claim.usage_right_ref))fail("InvalidFacilityRetentionPredecessor");auto close=take(read_facility_claim_receipt(state,*previous.close_receipt_ref));if(!same(close.occurred_at,transition.occurred_at,b)||close.source_result_ref.kind!="facility_retention"||close.source_result_ref.id!=transition.transition_id)fail("MissingFacilityRetentionAtomicClose");}
}
Outcome<bool> read_verified_facility_retention_class(const FacilityClaimsState& state,const FacilityClaim2& claim,const MomentSpan& window,const FacilityCapacitySource2& capacity,const EconomicFacilityOriginalSourceRead& reader,ExactArithmeticBudget b){return guard<bool>([&]{if(claim.source_result_ref.kind!="facility_retention")return false;FacilitySourceRef ref{claim.source_result_ref,1};auto proof=take(reader.read_facility_retention_proof(ref,claim.slot_ref,window));const auto& transition=proof.transition;if(transition.transition_id!=claim.source_result_ref.id||transition.source_schema!=1||transition.slot_ref!=claim.slot_ref)fail("FacilityRetentionSourceMismatch");Integer matches=0;bool retained=false;for(const auto& entry:transition.entries)std::visit([&](const auto& e){using T=std::decay_t<decltype(e)>;if constexpr(std::is_same_v<T,FacilityRetentionBegin>||std::is_same_v<T,FacilityRetentionContinue>){if(e.retained_claim_id!=claim.claim_id)return;++matches;retained=true;if(!same(e.retention_window.from,claim.from,b)||!same(e.retention_window.to,claim.until,b)||!covers(e.retention_window,window,b)||!same(claim.from,transition.occurred_at,b))fail("FacilityRetentionWindowMismatch");bool reduced=false;if constexpr(std::is_same_v<T,FacilityRetentionContinue>)reduced=e.clearance_source_ref.has_value();predecessor(state,claim,transition,e.previous_claim_id,e.origin_claim_id,reduced,window.to,b);auto prior=take(read_facility_claim_episode(state,e.previous_claim_id,window.to,b));if constexpr(std::is_same_v<T,FacilityRetentionBegin>){if(e.previous_claim_id!=e.origin_claim_id||claim.units!=prior.original_claim.units)fail("InvalidFacilityRetentionBegin");}else if(reduced!=(claim.units<prior.original_claim.units))fail("FacilityClearanceQuantityMismatch");retained_positions(state,claim,e.previous_claim_id,e.origin_claim_id,reduced,window.to,capacity,reader,b);if constexpr(std::is_same_v<T,FacilityRetentionBegin>){auto origin=take(read_facility_claim_episode(state,e.origin_claim_id,window.to,b));if(origin.admission_receipt_ref.slot_ref!=e.origin_admission_receipt_ref.slot_ref||origin.admission_receipt_ref.claim_set_revision!=e.origin_admission_receipt_ref.claim_set_revision)fail("FacilityRetentionOriginReceiptMismatch");}else{auto previous=take(read_facility_claim_episode(state,e.previous_claim_id,window.to,b));if(!same_source_ref(previous.original_claim.source_result_ref,e.previous_retention_source_ref))fail("FacilityRetentionChainMismatch");}}else if constexpr(std::is_same_v<T,FacilityRetentionRestoredService>){if(e.normal_claim_id!=claim.claim_id)return;++matches;retained=false;auto previous=take(read_facility_claim_episode(state,e.previous_claim_id,window.to,b));if(!previous.close_receipt_ref||previous.original_claim.holder!=claim.holder||previous.original_claim.slot_ref!=claim.slot_ref||!same_source_ref(previous.original_claim.source_result_ref,e.previous_retention_source_ref))fail("FacilityRestoredServicePredecessorMismatch");auto closed=take(read_facility_claim_receipt(state,*previous.close_receipt_ref));if(!same(closed.occurred_at,transition.occurred_at,b)||closed.source_result_ref.kind!="facility_retention"||closed.source_result_ref.id!=transition.transition_id||claim.units!=previous.original_claim.units||!same(claim.from,transition.occurred_at,b))fail("FacilityRestoredServiceAtomicCloseMissing");retained_positions(state,claim,e.previous_claim_id,e.origin_claim_id,false,window.to,capacity,reader,b);}},entry);if(matches!=1)fail("UnclassifiedFacilityRetentionClaim");if(retained){const auto* refs=retention_refs(capacity);if(!refs||std::none_of(refs->begin(),refs->end(),[&](const auto& r){return r.source_schema==1&&same_source_ref(r.source_ref,claim.source_result_ref);}))fail("MissingFacilityRetentionCapacityBinding");}return retained;});}
// 原布局先读一次；AggregateOnly没有physical binding，不能用虚构空binding过门。
Outcome<Integer> read_facility_additional_normal_capacity(const FacilityClaimsState& state,const FacilitySlotRef& slot,const MomentSpan& window,const std::vector<const FacilityClaim2*>& claims,const FacilityCapacitySource2& capacity,const EconomicFacilityOriginalSourceRead& reader,ExactArithmeticBudget b){return guard<Integer>([&]{
    if(capacity.slot_ref!=slot||capacity.capacity<0)fail("WrongPhysicalCapacitySlot");
    const FacilitySourceRef capacity_ref{capacity.source_fact_ref,1};
    const auto definition=take(reader.read_facility_slot_definition_at_source(capacity_ref,slot));
    if(definition.physical_capacity_identity.empty()||definition.capacity_unit_id!=capacity.capacity_unit_id)fail("FacilityPhysicalDefinitionMismatch");
    const auto layout=definition.physical_layout.index();
    const std::vector<FacilityPhysicalUnitDefinition>* units=nullptr;
    if(const auto* independent=std::get_if<IndependentFacilityUnits>(&definition.physical_layout))units=&independent->unit_definitions;
    if(const auto* shared=std::get_if<SharedFacilitySafetyZone>(&definition.physical_layout))units=&shared->unit_definitions;
    Integer installed=0,total=0,normal=0;
    std::set<StableId> unit_keys,position_keys;
    if(units){
        if(units->size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))fail("FacilityPhysicalCapacityOverflow");
        installed=static_cast<Integer>(units->size());
        for(const auto& unit:*units)if(unit.unit_key.empty()||unit.physical_position_key.empty()||unit.capacity_unit_id!=capacity.capacity_unit_id||!unit_keys.insert(unit.unit_key).second||!position_keys.insert(unit.physical_position_key).second)fail("InvalidFacilityPhysicalUnitDefinition");
        if(capacity.capacity>installed)fail("FacilityCapacityExceedsInstalledUnits");
    }
    const auto roster=units?take(reader.read_physical_units_at_capacity_source(capacity_ref,slot)):std::vector<FacilityPhysicalUnitRef>{};if(units&&roster.size()!=units->size())fail("FacilityPhysicalRosterCountMismatch");
    std::set<StableId> roster_keys;for(const auto& unit:roster)if(unit.facility_id!=slot.facility_id||!unit_keys.count(unit.unit_key)||!roster_keys.insert(unit.unit_key).second)fail("FacilityPhysicalRosterMismatch");
    std::set<StableId> occupied;
    for(const auto* claim:claims){
        if(!claim||claim->slot_ref!=slot)fail("WrongPhysicalClaimSlot");
        const bool retained=take(read_verified_facility_retention_class(state,*claim,window,capacity,reader,b));
        total=sum(total,claim->units);if(!retained)normal=sum(normal,claim->units);
        if(!units)continue;
        const auto proof=binding(state,*claim,window.to,reader,b);
        // 升级保原definition家族、安装身份与布局语义，旧binding继续其当时锁版，不追改版本。
        if(proof.definition.definition_ref.id!=definition.definition_ref.id||proof.definition.asset_kind!=definition.asset_kind||proof.definition.slot_kind!=definition.slot_kind||proof.definition.capacity_unit_id!=definition.capacity_unit_id||proof.definition.physical_layout.index()!=layout||proof.definition.physical_capacity_identity!=definition.physical_capacity_identity)fail("FacilityLayoutMismatch");
        const std::vector<FacilityPhysicalUnitDefinition>* original_units=nullptr;if(const auto* old=std::get_if<IndependentFacilityUnits>(&proof.definition.physical_layout))original_units=&old->unit_definitions;if(const auto* old=std::get_if<SharedFacilitySafetyZone>(&proof.definition.physical_layout))original_units=&old->unit_definitions;if(!original_units)fail("MissingOriginalPhysicalLayout");
        StableId previous_key;
        for(const auto& unit:proof.binding.physical_unit_refs){
            if(!previous_key.empty()&&unit.unit_key<=previous_key)fail("FacilityPhysicalUnitOrderMismatch");previous_key=unit.unit_key;
            const auto found=std::find_if(units->begin(),units->end(),[&](const auto& value){return value.unit_key==unit.unit_key;});
            if(found==units->end()||!occupied.insert(found->physical_position_key).second)fail("OverlappingFacilityPhysicalPosition");
            const auto prior=std::find_if(original_units->begin(),original_units->end(),[&](const auto& value){return value.unit_key==unit.unit_key;});if(prior==original_units->end()||prior->physical_position_key!=found->physical_position_key||prior->capacity_unit_id!=found->capacity_unit_id||std::none_of(roster.begin(),roster.end(),[&](const auto& current_unit){return same_unit(unit,current_unit);}))fail("FacilityPhysicalUnitIdentityChanged");
        }
    }
    if(units&&total>installed)fail("FacilityPhysicalBudgetExceeded");
    if(capacity.admission_disposition.index()==2){if(normal!=0)fail("ClosedFacilityHasNormalService");}
    else if(layout==1){if(normal>capacity.capacity)fail("IndependentFacilityBudgetExceeded");}
    else if(total>capacity.capacity)fail("AggregateFacilityBudgetExceeded");
    if(const auto* refs=retention_refs(capacity))for(const auto& ref:*refs){
        const auto proof=take(reader.read_facility_retention_proof(ref,slot,window));bool covers_retained=false;
        for(const auto& entry:proof.transition.entries)std::visit([&](const auto& e){using T=std::decay_t<decltype(e)>;if constexpr(std::is_same_v<T,FacilityRetentionBegin>||std::is_same_v<T,FacilityRetentionContinue>)for(const auto* claim:claims)if(claim->claim_id==e.retained_claim_id&&same_source_ref(claim->source_result_ref,ref.source_ref))covers_retained=true;},entry);
        if(!covers_retained)fail("ExtraneousFacilityRetentionSource");
    }
    if(capacity.admission_disposition.index()==2)return Integer{0};
    // 独立位同时受实际可服务C与物理B约束；无独立证明严格共同计费，不凭tag1增席。
    return layout==1?std::min(capacity.capacity-normal,installed-total):capacity.capacity-total;
});}
Status validate_facility_physical_interval(const FacilityClaimsState& state,const FacilitySlotRef& slot,const MomentSpan& window,const std::vector<const FacilityClaim2*>& claims,const FacilityCapacitySource2& capacity,const EconomicFacilityOriginalSourceRead& reader,ExactArithmeticBudget b){auto result=read_facility_additional_normal_capacity(state,slot,window,claims,capacity,reader,b);if(!result.ok)return {false,{},std::move(result.error)};return {true,{}, {}};}
}
