// 强制设施到期与真实容量变更都在原候选内走专业holder；仅技术暂停不会释放任何claim。
#include "facility_claim_runtime.hpp"
#include "facility_claims_candidate.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
void fail(const char* code,ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(code,"facility_claims",kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("FacilityRuntimeResourceExhausted","facility_claims",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("FacilityRuntimeResourceExhausted","facility_claims",ErrorKind::internal).error};}}
auto key(const FacilityHolderKind& k){return std::tie(k.kind,k.owner_domain,k.activity_kind);}
// 纯遍历完整登记，不为每次热反验复制拥有字符串的目录。
void registry_shape(const std::vector<FacilityHolderRegistration>& registry){for(std::size_t i=0;i<registry.size();++i){const auto& p=registry[i];if(static_cast<unsigned>(p.selection.holder.kind)>static_cast<unsigned>(ReservationRef::Kind::domain_activity)||p.selection.profile_id.empty()||p.selection.version<=0||p.selection.holder.owner_domain.empty()||p.selection.holder.activity_kind.empty()||!p.until_stage||static_cast<unsigned>(*p.until_stage)>static_cast<unsigned>(ExactStage::achievement)||!p.apply_until||!p.reconcile_capacity)fail("IncompleteFacilityHolderAdapter",ErrorKind::unsupported_capability);for(std::size_t j=0;j<i;++j)if(key(registry[j].selection.holder)==key(p.selection.holder))fail("DuplicateFacilityHolderAdapter");}}
Integer revision(const FacilityClaimsState& state,const FacilitySlotRef& slot){for(const auto& h:state.slot_histories)if(h.slot_ref==slot){if(!h.mutations.empty())return h.mutations.back().claim_set_revision;if(const auto* base=std::get_if<VacantPrunedFacilityPrefix>(&h.base))return base->last_removed_revision;return 0;}return 0;}
const FacilityHolderRegistration& select(const ReservationRef& holder,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& source){auto actual=take(source.read_facility_holder_kind(holder));if(actual.kind!=holder.kind||actual.owner_domain.empty()||actual.activity_kind.empty())fail("InvalidFacilityHolderKind");if(holder.kind==ReservationRef::Kind::domain_activity&&(actual.owner_domain!=holder.domain_id||actual.activity_kind!=holder.activity_kind))fail("FacilityDomainHolderMismatch");const FacilityHolderRegistration* found=nullptr;for(const auto& p:registry)if(key(p.selection.holder)==key(actual)){if(found)fail("DuplicateFacilityHolderAdapter");found=&p;}if(!found||!found->apply_until||!found->reconcile_capacity)fail("MissingFacilityHolderAdapter",ErrorKind::unsupported_capability);return *found;}
class HolderWriteBoundary {WorldBundle& world_;public:explicit HolderWriteBoundary(WorldBundle& w):world_(w){world_.note_write_boundary();}~HolderWriteBoundary(){world_.note_write_boundary();}};
template<class F>auto write_holder(WorldBundle& world,F fn)->decltype(fn()){HolderWriteBoundary boundary(world);return fn();}
bool closes(const FacilityClaimMutationReceipt& r,const StableId& id){return std::any_of(r.closed_claims.begin(),r.closed_claims.end(),[&](const auto& c){return c.claim_id==id;});}
bool same_claim(const FacilityClaim2& a,const FacilityClaim2& z,ExactArithmeticBudget b){
    return a.claim_id==z.claim_id&&a.slot_ref==z.slot_ref&&a.holder==z.holder&&
        take(compare_world_moments(a.from,z.from,b))==0&&take(compare_world_moments(a.until,z.until,b))==0&&
        a.units==z.units&&a.capacity_revision==z.capacity_revision&&
        same_source_ref(a.usage_right_ref,z.usage_right_ref)&&same_source_ref(a.source_result_ref,z.source_result_ref);
}
// One original ordering implementation for both actual capacity and actual
// claim-until causes. Future reservations cannot reset another holder's budget.
std::vector<FacilityClaim2> ordered_slot_claims(const FacilityClaimsState& state,const FacilitySlotRef& slot,const WorldMoment& at,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& holders,ExactArithmeticBudget b){
    struct Ordered{FacilityClaim2 claim;bool started=false;};std::vector<Ordered> planning;
    for(const auto& c:state.claims)if(c.slot_ref==slot){select(c.holder,registry,holders);const auto started=take(holders.has_actual_facility_service_started(c,at));if(started&&take(compare_world_moments(c.from,at,b))>0)fail("FacilityFutureClaimCannotHaveStarted");planning.push_back({c,started});}
    std::sort(planning.begin(),planning.end(),[&](const auto& a,const auto& z){if(a.started!=z.started)return a.started;const auto order=take(compare_world_moments(a.claim.from,z.claim.from,b));return order?order<0:a.claim.claim_id<z.claim.claim_id;});
    std::vector<FacilityClaim2> ordered;ordered.reserve(planning.size());for(auto& p:planning)ordered.push_back(std::move(p.claim));return ordered;
}
bool same_capacity_binding(const FacilityClaim2& prepared,const FacilityClaim2& actual){return prepared.capacity_revision==actual.capacity_revision;}
bool same_capacity_binding(const PreparedFacilityClaimAdmission&,const FacilityClaim2& actual){
    // The exact revision is a result of the real finalizer. Full original source
    // validation in the unique S0 adoption proves it against actual capacity.
    return actual.capacity_revision>0;
}
template<class OwnerResult>void validate_final_owner_coverage(const std::vector<OwnerResult>& results,const FacilityClaimMutationReceipt& receipt,const FacilitySlotRef& slot,const WorldMoment& at,Integer before,const FacilityClaimSourceRead& source,ExactArithmeticBudget b){
    if(receipt.claim_set_revision!=before+1||take(compare_world_moments(receipt.occurred_at,at,b))!=0)fail("FacilityReconcileSourceMismatch");
    using Admissions=std::decay_t<decltype(std::declval<OwnerResult>().admitted_claims)>;
    using Admission=typename Admissions::value_type;
    std::map<StableId,const ClosedFacilityClaim*> expected_closed;std::map<StableId,const Admission*> expected_admitted;
    for(const auto& result:results){for(const auto& closed:result.closed_claims)if(!expected_closed.emplace(closed.claim_id,&closed).second)fail("DuplicateFacilityReconcileClose");for(const auto& admitted:result.admitted_claims)if(!expected_admitted.emplace(admitted.claim_id,&admitted).second)fail("DuplicateFacilityReconcileAdmission");}
    if(receipt.closed_claims.size()!=expected_closed.size()||receipt.admitted_claims.size()!=expected_admitted.size())fail("FacilityReconcileOwnerResultCoverage");
    for(const auto& closed:receipt.closed_claims){auto old=expected_closed.find(closed.claim_id);if(old==expected_closed.end()||old->second->reason!=closed.reason)fail("FacilityReconcileOwnerResultCoverage");}
    for(const auto& admitted:receipt.admitted_claims){auto old=expected_admitted.find(admitted.claim_id);if(old==expected_admitted.end())fail("FacilityReconcileOwnerResultCoverage");const auto& original=*old->second;if(admitted.slot_ref!=original.slot_ref||admitted.holder!=original.holder||admitted.units!=original.units||!same_capacity_binding(original,admitted)||!same_source_ref(admitted.usage_right_ref,original.usage_right_ref)||take(compare_world_moments(admitted.from,original.from,b))!=0||take(compare_world_moments(admitted.until,original.until,b))!=0||!same_source_ref(admitted.source_result_ref,receipt.source_result_ref))fail("FacilityReconcileOwnerResultCoverage");}
    auto primary=take(source.capacity_source_at(slot,at));bool binds=false;for(const auto& ref:receipt.capacity_source_refs)if(ref.source_schema==primary.source_schema&&same_source_ref(ref.source_ref,primary.source_ref))binds=true;if(!binds)fail("MissingFacilityChangedCapacitySource");
}
}
Outcome<std::vector<FacilityHolderPhaseSelection>> compile_facility_holder_profiles(const std::vector<FacilityHolderRegistration>& registry){return guard<std::vector<FacilityHolderPhaseSelection>>([&]{
    std::vector<FacilityHolderPhaseSelection> out;out.reserve(registry.size());
    registry_shape(registry);for(const auto& p:registry)out.push_back({p.selection,*p.until_stage});return out;
});}
Status validate_facility_holder_registry(const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& source){return guard<Done>([&]{registry_shape(registry);for(const auto& p:registry)take(source.validate_facility_holder_selection(p.selection));return Done{};});}
Outcome<std::vector<FacilityClaimDue>> predict_facility_claim_dues(const FacilityClaimsState& state,const WorldMoment& from,const WorldMoment& through,ExactArithmeticBudget b){return guard<std::vector<FacilityClaimDue>>([&]{if(take(compare_world_moments(from,through,b))>0)fail("InvalidFacilityPredictionWindow");std::vector<FacilityClaimDue> out;for(const auto& c:state.claims){if(take(compare_world_moments(c.until,from,b))<0)fail("UnconsumedFacilityClaimDue");if(take(compare_world_moments(c.until,through,b))<=0)out.push_back({c.claim_id,c.slot_ref,revision(state,c.slot_ref),c.until});}std::sort(out.begin(),out.end(),[&](const auto& a,const auto& z){auto cmp=take(compare_world_moments(a.due_at,z.due_at,b));return cmp?cmp<0:a.claim_id<z.claim_id;});return out;});}
Outcome<FacilityClaimReceiptRef> apply_facility_claim_due_in_candidate(WorldBundle& world,FacilityClaimsState& state,const FacilityClaimsCatalog& catalog,const FacilityClaimDue& due,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& holders,const FacilityClaimSourceRead& source,ExactPhaseWriteContext& writes,ExactArithmeticBudget b){return guard<FacilityClaimReceiptRef>([&]{
    if(!world.facility_claims||&state!=&*world.facility_claims)fail("FacilityCandidateRootMismatch");
    if(revision(state,due.slot_ref)!=due.claim_set_revision)fail("StaleFacilityDuePrediction");const FacilityClaim2* found=nullptr;for(const auto& claim:state.claims)if(claim.claim_id==due.claim_id){if(found)fail("DuplicateFacilityClaim");found=&claim;}if(!found||found->slot_ref!=due.slot_ref||take(compare_world_moments(found->until,due.due_at,b))!=0)fail("StaleFacilityDuePrediction");
    // 原callback可能改业务表并分新ID；保一份小的原claim深值，绝不借用vector失效指针。
    const auto original=*found;const auto& owner=select(original.holder,registry,holders);auto receipt=take(write_holder(world,[&]{return owner.apply_until(world,original,due.due_at,writes);}));
    if(!closes(receipt,original.claim_id))fail("FacilityDueNotResolved");if(revision(state,due.slot_ref)!=due.claim_set_revision)fail("HolderMutatedSharedFacilityState");
    take(adopt_facility_claim_mutation_in_candidate(world,catalog,due.slot_ref,receipt,due.claim_set_revision,due.due_at,source,b));return FacilityClaimReceiptRef{due.slot_ref,receipt.claim_set_revision};
});}
Outcome<std::optional<FacilityClaimMutationReceipt>> prepare_facility_capacity_reconciliation_in_candidate(WorldBundle& world,const FacilityClaimsState& state,const FacilityCapacityReconciliationRequest& request,const FacilityCapacityReconciliationRegistration& provider,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& holders,const FacilityClaimSourceRead& source,ExactPhaseWriteContext& writes,ExactArithmeticBudget b){return guard<std::optional<FacilityClaimMutationReceipt>>([&]{
    if(!world.facility_claims||&state!=&*world.facility_claims)fail("FacilityCandidateRootMismatch");
    const auto& slot=request.slot_ref;const auto at=request.occurred_at;
    if(provider.provider_domain!=slot.provider_domain||provider.profile_id.empty()||provider.profile_version<=0||!provider.validate_request||!provider.finalize||!valid_source_ref(request.original_change.source_ref)||request.original_change.source_schema<=0)fail("MissingFacilityCapacityReconciler",ErrorKind::unsupported_capability);
    take(provider.validate_request(world,request));const auto before=revision(state,slot);if(before==std::numeric_limits<Integer>::max())fail("FacilityClaimRevisionOverflow");
    auto ordered=ordered_slot_claims(state,slot,at,registry,holders,b);std::vector<FacilityClaimMutationReceipt> results;
    for(std::size_t position=0;position<ordered.size();++position){const auto& original=ordered[position];const auto& handler=select(original.holder,registry,holders);FacilityReconciliationProgress progress{ordered,results,position};auto result=take(write_holder(world,[&]{return handler.reconcile_capacity(world,request,progress,writes);}));
        if(result.claim_set_revision!=before+1||take(compare_world_moments(result.occurred_at,at,b))!=0||!valid_source_ref(result.source_result_ref)||result.source_schema<=0)fail("FacilityReconcileSourceMismatch");
        for(const auto& closed:result.closed_claims)if(closed.claim_id!=original.claim_id)fail("FacilityHolderClosedForeignClaim");for(const auto& admitted:result.admitted_claims)if(admitted.holder!=original.holder||admitted.slot_ref!=slot)fail("FacilityHolderAdmittedForeignClaim");if(revision(state,slot)!=before)fail("HolderMutatedSharedFacilityState");results.push_back(std::move(result));
    }
    // 原provider在全部实际处置后才分配最终root/capacity/物理binding，S0没有retention影子表。
    auto finalized=take(write_holder(world,[&]{return provider.finalize(world,request,results,writes);}));if(revision(state,slot)!=before)fail("HolderMutatedSharedFacilityState");
    if(ordered.empty()){if(finalized)fail("EmptyFacilityReconcileMutation");return std::optional<FacilityClaimMutationReceipt>{};}
    if(!finalized)fail("MissingFacilityReconcileMutation");const auto& receipt=*finalized;
    validate_final_owner_coverage(results,receipt,slot,at,before,source,b);
    return finalized;
});}
Outcome<FacilityClaimMutationReceipt> prepare_facility_until_reconciliation_in_candidate(WorldBundle& world,const FacilityClaimsState& state,const FacilityClaimUntilRequest& request,const FacilityClaimUntilReconciliationRegistration& provider,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& holders,const FacilityClaimSourceRead& source,ExactPhaseWriteContext& writes,ExactArithmeticBudget b){return guard<FacilityClaimMutationReceipt>([&]{
    if(!world.facility_claims||&state!=&*world.facility_claims)fail("FacilityCandidateRootMismatch");
    const auto& original_due=request.due_claim;const auto& slot=original_due.slot_ref;const auto& at=request.occurred_at;
    if(take(compare_world_moments(take(world.actual_phase_moment()),at,b))||take(compare_world_moments(original_due.until,at,b)))fail("FacilityUntilActualMomentMismatch",ErrorKind::internal);
    const FacilityClaim2* active=nullptr;for(const auto& c:state.claims)if(c.claim_id==original_due.claim_id){if(active)fail("DuplicateFacilityClaim");active=&c;}
    if(!active||!same_claim(*active,original_due,b))fail("FacilityUntilOriginalClaimMismatch",ErrorKind::internal);
    const auto episode=take(read_facility_claim_episode(state,original_due.claim_id,at,b));
    if(episode.close_receipt_ref||!same_claim(episode.original_claim,original_due,b)||
       episode.admission_receipt_ref.slot_ref!=request.admission_receipt_ref.slot_ref||
       episode.admission_receipt_ref.claim_set_revision!=request.admission_receipt_ref.claim_set_revision)
        fail("FacilityUntilOriginalAdmissionMismatch",ErrorKind::internal);
    if(provider.provider_domain!=slot.provider_domain||provider.profile_id.empty()||provider.profile_version<=0||!provider.validate_request||!provider.finalize_until)fail("MissingFacilityUntilReconciler",ErrorKind::unsupported_capability);
    take(provider.validate_request(world,request));const auto before=revision(state,slot);if(before==std::numeric_limits<Integer>::max())fail("FacilityClaimRevisionOverflow");
    auto ordered=ordered_slot_claims(state,slot,at,registry,holders,b);std::vector<PreparedFacilityClaimMutation> results;
    for(std::size_t position=0;position<ordered.size();++position){
        const auto& original=ordered[position];const auto& handler=select(original.holder,registry,holders);
        if(!handler.reconcile_until)fail("MissingFacilityHolderUntilReconciler",ErrorKind::unsupported_capability);
        FacilityUntilReconciliationProgress progress{ordered,results,position};
        auto result=take(write_holder(world,[&]{return handler.reconcile_until(world,request,progress,writes);}));
        if(result.claim_set_revision!=before+1||take(compare_world_moments(result.occurred_at,at,b))||!valid_source_ref(result.source_result_ref)||result.source_schema<=0)fail("FacilityUntilOwnerSourceMismatch",ErrorKind::internal);
        for(const auto& closed:result.closed_claims)if(closed.claim_id!=original.claim_id)fail("FacilityHolderClosedForeignClaim");
        for(const auto& admitted:result.admitted_claims){
            if(admitted.claim_id.empty()||admitted.holder!=original.holder||admitted.slot_ref!=slot)fail("FacilityHolderAdmittedForeignClaim");
            if(admitted.units<=0||!valid_source_ref(admitted.usage_right_ref)||!same_source_ref(admitted.source_result_ref,result.source_result_ref)||take(compare_world_moments(admitted.from,at,b))<0||take(compare_world_moments(admitted.from,admitted.until,b))>=0)fail("InvalidPreparedFacilityAdmission",ErrorKind::internal);
        }
        if(revision(state,slot)!=before)fail("HolderMutatedSharedFacilityState");results.push_back(std::move(result));
    }
    auto finalized=take(write_holder(world,[&]{return provider.finalize_until(world,request,results,writes);}));
    if(revision(state,slot)!=before)fail("HolderMutatedSharedFacilityState");
    if(!valid_source_ref(finalized.source_result_ref)||finalized.source_schema<=0)fail("FacilityUntilFinalSourceRequired",ErrorKind::internal);
    validate_final_owner_coverage(results,finalized,slot,at,before,source,b);
    if(!closes(finalized,original_due.claim_id))fail("FacilityDueNotResolved");
    // No S0 mutation here. Its existing outer apply_until adopts this one fully
    // bound result and runs the complete original capacity/admission validator.
    return finalized;
});}
Outcome<std::vector<FacilityClaimReceiptRef>> reconcile_facility_capacity_in_candidate(WorldBundle& world,FacilityClaimsState& state,const FacilityClaimsCatalog& catalog,const FacilityCapacityReconciliationRequest& request,const FacilityCapacityReconciliationRegistration& provider,const std::vector<FacilityHolderRegistration>& registry,const FacilityHolderSourceRead& holders,const FacilityClaimSourceRead& source,ExactPhaseWriteContext& writes,ExactArithmeticBudget b){return guard<std::vector<FacilityClaimReceiptRef>>([&]{
    const auto before=revision(state,request.slot_ref);
    auto finalized=take(prepare_facility_capacity_reconciliation_in_candidate(world,state,request,provider,registry,holders,source,writes,b));
    if(!finalized)return std::vector<FacilityClaimReceiptRef>{};
    const auto& receipt=*finalized;
    // The original public entry still adopts exactly once, after every actual
    // holder result and the finalizer have succeeded in this same candidate.
    take(adopt_facility_claim_mutation_in_candidate(world,catalog,request.slot_ref,receipt,before,request.occurred_at,source,b));
    return std::vector<FacilityClaimReceiptRef>{{request.slot_ref,receipt.claim_set_revision}};
});}
}
