#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "json_decoded_world_seed.hpp"
#include "world_bundle.hpp"
#include "world_retention.hpp"
#include "catalog_retention.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
// Copies of this deleter share the same original payload and reservation.
// Both member destruction and the final strong-release callback drop the
// catalog before its ordinary token. The alias control is separately tracked.
struct JsonDecodedCatalogDelete {
    std::shared_ptr<CheckpointJobReservation> ordinary;
    std::shared_ptr<const PreparedCatalog> original;
    CheckpointAllocator<PreparedCatalog> allocator;
    void operator()(const PreparedCatalog*)noexcept{original.reset();ordinary.reset();}
};
}
std::shared_ptr<const PreparedCatalog> seal_json_decoded_catalog(std::shared_ptr<const PreparedCatalog> catalog,CodecDecodeBudget& budget,CheckpointJobBudget& job){
    if(!catalog||!budget.belongs_to(job))throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
    auto active=budget.account_allocator_candidate(0,current_checkpoint_memory());
    if(!active.ok)throw checkpoint_job_detail::FailureException{job.capture_failure(std::move(active.error))};
    // Finish every fallible object/control allocation while the original
    // budget still owns its reservation. The final token move cannot fail.
    auto ordinary=checkpoint_make_shared<CheckpointJobReservation>();
    CheckpointAllocator<PreparedCatalog> allocator;
    const auto* value=catalog.get();
    std::shared_ptr<const PreparedCatalog> result(value,JsonDecodedCatalogDelete{ordinary,std::move(catalog),allocator},allocator);
    *ordinary=budget.take_job_reservation();
    return result;
}
bool has_json_decoded_catalog_owner(const std::shared_ptr<const PreparedCatalog>& catalog)noexcept{
    return std::get_deleter<JsonDecodedCatalogDelete>(catalog)!=nullptr;
}
Status retain_json_decoded_catalog_owner(const std::shared_ptr<const PreparedCatalog>& catalog,CheckpointRetentionCensus& census){
    const auto* owner=std::get_deleter<JsonDecodedCatalogDelete>(catalog);
    if(!owner||!owner->ordinary||!owner->ordinary->valid()||!owner->original||catalog.get()!=owner->original.get())return common_failure("JsonDecodedCatalogOwnerMismatch",{},ErrorKind::internal);
    census.owner(owner->allocator.allocation_owner());
    census.owner(checkpoint_shared_allocation_owner(owner->ordinary));
    if(!census.accounts(*owner->ordinary))owner->ordinary->retain_control(census);
    // Original graph and original untracked-owner rejection, not a second
    // field walker or a blanket credit for manual catalog allocations.
    return retain_prepared_catalog_owned_graph(owner->original,census);
}
class JsonDecodedSeedState {
public:
    // 费用先构造最后析构；Common移入新World之后仍由这个同一State持费。
    CheckpointJobReservation memory;
    std::shared_ptr<CheckpointMemoryResource> resource;
    std::shared_ptr<const PreparedCatalog> catalog;
    InitialWorldSeed original;
    bool sealed=false;bool consumed=false;
    JsonDecodedSeedState(CheckpointJobReservation&& token,std::shared_ptr<CheckpointMemoryResource> allocator,std::shared_ptr<const PreparedCatalog> prepared)
        :memory(std::move(token)),resource(std::move(allocator)),catalog(std::move(prepared)){}
};
CheckpointJobResult<JsonDecodedWorldSeedOwner> JsonDecodedWorldSeedOwner::begin(std::shared_ptr<const PreparedCatalog> catalog,CodecDecodeBudget& budget,CheckpointJobBudget& job){
    if(!catalog||!budget.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
    auto profile=job.check_profile(budget.profile);if(!profile.ok)return {false,{},std::move(profile.error)};
    auto token=job.reserve(CheckpointJobUse::candidate,0);if(!token.ok)return {false,{},std::move(token.error)};
    auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};
    try{ScopedCheckpointMemory scope(resource.value);
        // All other InitialWorldSeed members are empty shared_ptr handles.
        auto defaults=charge_checkpoint_defaults<CommonState>(budget,1);if(!defaults.ok)return {false,{},job.capture_failure(std::move(defaults.error))};
        JsonDecodedWorldSeedOwner out;
        out.state_=checkpoint_make_shared<JsonDecodedSeedState>(std::move(token.value),resource.value,std::move(catalog));
        return {true,std::move(out),{}};
    }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}
    catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
InitialWorldSeed& JsonDecodedWorldSeedOwner::candidate(){if(!state_||state_->sealed)throw std::logic_error("PrivateJsonDecodedSeedRequired");return state_->original;}
std::shared_ptr<CheckpointMemoryResource> JsonDecodedWorldSeedOwner::memory_resource()const noexcept{return state_?state_->resource:nullptr;}
CheckpointJobStatus JsonDecodedWorldSeedOwner::seal(CodecDecodeBudget& budget,CheckpointJobBudget& job){
    if(!state_||state_->sealed||!budget.belongs_to(job)||!state_->memory.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
    auto active=budget.account_allocator_candidate(0,state_->resource);if(!active.ok)return {false,{},job.capture_failure(std::move(active.error))};
    // Transfer only charges actually collected by this budget. Installation
    // may normalize fixed identity scalars, but cannot back-credit raw payload.
    // Existing reservation moves intact; never release the old candidate first.
    state_->memory=budget.take_job_reservation();state_->sealed=true;return {true,{},{}};
}
std::shared_ptr<const PreparedCatalog> JsonDecodedWorldSeedOwner::require_catalog(std::shared_ptr<const PreparedCatalog> catalog)const{
    if(!state_||!state_->sealed||!catalog||catalog.get()!=state_->catalog.get())throw std::logic_error("JsonDecodedSeedCatalogMismatch");return catalog;
}
InitialWorldSeed& JsonDecodedWorldSeedOwner::install_seed(){if(!state_||!state_->sealed||state_->consumed)throw std::logic_error("PrivateJsonDecodedSeedRequired");return state_->original;}
InitialWorldSeed&& JsonDecodedWorldSeedOwner::consume_seed(){if(!state_||!state_->sealed||state_->consumed)throw std::logic_error("PrivateJsonDecodedSeedRequired");state_->consumed=true;return std::move(state_->original);}
std::shared_ptr<const JsonDecodedSeedState> JsonDecodedWorldSeedOwner::release_owner()noexcept{return std::move(state_);}
Status retain_json_decoded_seed_owner(const std::shared_ptr<const JsonDecodedSeedState>& state,const CommonState& adopted_common,CheckpointRetentionCensus& census){
    if(!state)return common_success();
    if(!state->sealed)return common_failure("PrivateJsonDecodedSeedRequired",{},ErrorKind::internal);
    census.owner(checkpoint_shared_allocation_owner(state));census.resource(state->resource);
    auto catalog=retain_prepared_catalog(state->catalog,census);if(!catalog.ok)return catalog;
    if(!census.accounts(state->resource))state->memory.retain_control(census);
    // This partial state token is deliberately not a free credit for
    // the seed graph. Census still enumerates actual allocations and may be
    // conservative; it cannot retroactively pay raw decoder first copies.
    // 原预像真实保留到该World死亡。clone有独立费用，不继承这个State。
    return retain_initial_world_seed_graph(state->original,&adopted_common,census);
}
}
