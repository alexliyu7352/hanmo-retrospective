#include "native_complete_facility_holders.hpp"
#include "native_complete_time_catalog.hpp"
#include "native_item_craft_state.hpp"
#include "world_bundle.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/craft/repair_contract.hpp"
#include "resident/domains/craft/craft_facility_runtime.hpp"
#include "resident/domains/local_economy/household_service_runtime.hpp"
#include <iterator>
#include <utility>
namespace hanmo::resident { namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){
    throw Failure{common_failure(code,id,ErrorKind::unsupported_capability).error};
}
template<class T>T take(Outcome<T> value){
    if(!value.ok)throw Failure{std::move(value.error)};
    return std::move(value.value);
}
template<class T,class F>Outcome<T> guarded(F&& fn){
    try{return {true,fn(),{}};}catch(Failure& error){return {false,{},std::move(error.error)};}
}
}
NativeCompleteFacilityHolderRead::NativeCompleteFacilityHolderRead(
    const FacilityClaimsState& claims,const WorldMoment& current,
    const CraftJobsState* craft_jobs,const ItemInstancesState* repair_items,
    const HouseholdPurchaseState* household,ExactArithmeticBudget arithmetic)
    :claims_(claims),current_(current),craft_jobs_(craft_jobs),repair_items_(repair_items),
     household_purchases_(household),arithmetic_(std::move(arithmetic)){}
void NativeCompleteFacilityHolderRead::require_bound()const{
    if(!bound_)fail("CompleteFacilityHolderSourcesNotBound");
}
Status NativeCompleteFacilityHolderRead::bind_sources(
    const CraftFacilityOccupancyContext* craft,const HouseholdPurchaseContext* household){
    return guarded<Done>([&]{
        if(bound_)fail("CompleteFacilityHolderSourcesAlreadyBound");
        if(bool(craft)!=bool(craft_jobs_||repair_items_)||bool(household)!=bool(household_purchases_))
            fail("CompleteFacilityHolderSelectionMismatch");
        if(craft){
            if(take(compare_world_moments(take(craft->craft.clock.read_current_moment()),current_,arithmetic_)))
                fail("CompleteCraftFacilityMomentMismatch");
            // The current original placement owner lives in CraftJobsState.
            // Repair-only has no independent original placement owner yet;
            // do not satisfy that gap by manufacturing an unselected Craft root.
            if(!craft_jobs_)fail("CompleteRepairFacilityIndependentOwnerRequired");
            if(&craft->craft.materials.jobs!=craft_jobs_||&craft->claims!=&claims_||
               bool(craft->repair)!=bool(repair_items_))fail("CompleteCraftFacilityRootMismatch");
            if(repair_items_&&(&craft->repair->items!=repair_items_||
               &craft->craft.materials.instances!=repair_items_))fail("CompleteRepairFacilityRootMismatch");
        }
        if(household&&(&household->state!=household_purchases_||&household->claims!=&claims_||
           take(compare_world_moments(household->now,current_,arithmetic_))))
            fail("CompleteHouseholdFacilityRootMismatch");
        if(craft&&household&& &craft->craft.materials.common!=&household->common)
            fail("CompleteFacilityCommonRootMismatch");
        if(craft)craft_.emplace(*craft);
        if(household)household_.emplace(*household);
        bound_=true;
        return Done{};
    });
}
const FacilityHolderSourceRead& NativeCompleteFacilityHolderRead::specialist(const ReservationRef& holder)const{
    require_bound();
    bool crafting=false;
    if(craft_jobs_)for(const auto& row:craft_jobs_->jobs.rows())
        if(row.second.terms.material_holder==holder)crafting=true;
    if(repair_items_)for(const auto& row:repair_items_->repair_jobs.jobs.rows())
        if(row.second.terms.service_holder==holder)crafting=true;
    const bool household=household_purchases_&&holder.kind==ReservationRef::Kind::domain_activity&&
        holder.domain_id=="local_economy"&&holder.activity_kind=="household_purchase"&&
        (household_purchases_->purchases.find(holder.id)||household_purchases_->plans.find(holder.id));
    // Craft and Repair share one actual specialist. A contract key is never
    // interpreted as a Craft job ID or classified from a guessed domain tag.
    if(crafting&&household)fail("AmbiguousCompleteFacilityHolder",holder.id);
    if(crafting)return *craft_;
    if(household)return *household_;
    fail("CompleteOriginalFacilityHolderRequired",holder.id);
}
Outcome<FacilityHolderKind> NativeCompleteFacilityHolderRead::read_facility_holder_kind(const ReservationRef& holder)const{
    return guarded<FacilityHolderKind>([&]{return take(specialist(holder).read_facility_holder_kind(holder));});
}
Status NativeCompleteFacilityHolderRead::validate_facility_holder_selection(const FacilityHolderProfileSelection& selection)const{
    return guarded<Done>([&]{
        require_bound();
        if(selection.holder.owner_domain=="crafting"&&craft_){
            if((selection.holder.activity_kind=="craft_job"&&!craft_jobs_)||
               (selection.holder.activity_kind=="repair"&&!repair_items_))fail("UnselectedCompleteFacilityHolderProfile",selection.profile_id);
            return take(craft_->validate_facility_holder_selection(selection));
        }
        if(selection.holder.owner_domain=="local_economy"&&household_)
            return take(household_->validate_facility_holder_selection(selection));
        fail("UnsupportedCompleteFacilityHolderProfile",selection.profile_id);
    });
}
Outcome<std::vector<FacilityClaimDependency>> NativeCompleteFacilityHolderRead::enumerate_holder_claim_dependencies(const FacilitySlotRef& slot)const{
    return guarded<std::vector<FacilityClaimDependency>>([&]{
        require_bound();
        std::vector<FacilityClaimDependency> out;
        if(craft_)out=take(craft_->enumerate_holder_claim_dependencies(slot));
        if(household_){
            auto next=take(household_->enumerate_holder_claim_dependencies(slot));
            out.insert(out.end(),std::make_move_iterator(next.begin()),std::make_move_iterator(next.end()));
        }
        return out;
    });
}
Status NativeCompleteFacilityHolderRead::validate_holder_claim_admission(const FacilityClaim2& claim,const FacilityClaimMutationReceipt& receipt)const{
    return guarded<Done>([&]{return take(specialist(claim.holder).validate_holder_claim_admission(claim,receipt));});
}
Status NativeCompleteFacilityHolderRead::validate_holder_claim_close(const FacilityClaim2& claim,FacilityClaimCloseReason reason,const FacilityClaimMutationReceipt& receipt)const{
    return guarded<Done>([&]{return take(specialist(claim.holder).validate_holder_claim_close(claim,reason,receipt));});
}
Status NativeCompleteFacilityHolderRead::validate_holder_mutation_source(const FacilitySlotRef& slot,const FacilityClaimMutationReceipt& receipt)const{
    return guarded<Done>([&]{
        require_bound();
        // These are the changed admissions / closures, not a whole before/after
        // claim set. Select a specialist, then pass the ENTIRE original receipt.
        // Its existing validator, not this router, decides cross-claim legality.
        if(!receipt.admitted_claims.empty())return take(specialist(receipt.admitted_claims.front().holder).validate_holder_mutation_source(slot,receipt));
        if(!receipt.closed_claims.empty()){
            const auto episode=take(read_facility_claim_episode(claims_,receipt.closed_claims.front().claim_id,current_,arithmetic_));
            return take(specialist(episode.original_claim.holder).validate_holder_mutation_source(slot,receipt));
        }
        // Exact original specialist domains only; original kind/schema gates
        // remain in the reader. Physical-provider fallback remains its caller's
        // responsibility and is not converted into an empty successful receipt.
        if(receipt.source_result_ref.domain=="crafting"&&craft_)
            return take(craft_->validate_holder_mutation_source(slot,receipt));
        if(receipt.source_result_ref.domain=="local_economy"&&household_)
            return take(household_->validate_holder_mutation_source(slot,receipt));
        fail("UnsupportedCompleteFacilityMutationSource",receipt.source_result_ref.id);
    });
}
Outcome<bool> NativeCompleteFacilityHolderRead::has_actual_facility_service_started(const FacilityClaim2& claim,const WorldMoment& at)const{
    return guarded<bool>([&]{return take(specialist(claim.holder).has_actual_facility_service_started(claim,at));});
}
Outcome<std::vector<FacilityHolderRegistration>> read_native_complete_facility_holder_registrations(const PreparedCatalog& catalog){
    return guarded<std::vector<FacilityHolderRegistration>>([&]{
        if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))fail("CompleteExactProfileRequired");
        take(validate_native_item_craft_catalogs(catalog));
        const auto selected=take(selected_native_item_craft_versions(catalog.identity));
        std::vector<FacilityHolderRegistration> out;
        for(const auto kind:{ReservationRef::Kind::contract,ReservationRef::Kind::domain_activity}){
            if(selected.craft_jobs)out.push_back(make_craft_facility_holder_registration(kind,OriginalCraftFacilityHolder::craft_job,native_craft_facility_world_factory()));
            if(selected.item_instances==3)out.push_back(make_craft_facility_holder_registration(kind,OriginalCraftFacilityHolder::repair,native_craft_facility_world_factory()));
        }
        if(catalog.household_purchases){
            if(!catalog.household_retail_settlements||(catalog.selected_household_version!=2&&catalog.selected_household_version!=3))fail("HouseholdCatalogSelectionMismatch");
            out.push_back(make_household_service_holder_registration(native_household_service_world_factory()));
        }else if(catalog.household_retail_settlements||catalog.selected_household_version)fail("HouseholdCatalogSelectionMismatch");
        // The original compiler rejects missing actual callbacks / duplicate
        // keys. Household does not claim Craft's optional until-review callback.
        take(compile_facility_holder_profiles(out));
        return out;
    });
}
}
