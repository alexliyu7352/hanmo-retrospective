#include "world_restore_retention.hpp"
#include "world_impl.hpp"
namespace hanmo::resident {
WorldRestoreRetention::WorldRestoreRetention(ConstructionKey,CheckpointJobReservation&& retained,std::shared_ptr<const WorldBundle> world,std::optional<WorldIdentity> expected)noexcept
    :retained_(std::move(retained)),world_(std::move(world)),expected_(std::move(expected)){}
// 租约只从当前同一 world 取源；先核真实 owner 和稳定缓存，再增加跨 worker 的 shared 拥有者。
CheckpointJobResult<std::shared_ptr<const WorldRestoreRetention>> ResidentWorld::retain_storage_world(const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{
        if(!impl_->active_storage_restore.expired())return {false,{},job.capture_failure(common_failure("StorageRestoreBusy",{},ErrorKind::internal).error)};
        auto valid=job.check_profile(capacity);if(!valid.ok)return {false,{},std::move(valid.error)};valid=job.check_profile(slot);if(!valid.ok)return {false,{},std::move(valid.error)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        auto scratch=checkpoint_memory_resource(job,CheckpointJobUse::scratch);if(!scratch.ok)return {false,{},std::move(scratch.error)};
        CheckpointRetentionCensus census(scratch.value,CheckpointJobUse::candidate);std::optional<WorldIdentity> expected;
        if(impl_->world){
            auto ready=impl_->world->prepare_checkpoint_read_cache();if(!ready.ok)return {false,{},job.capture_failure(std::move(ready.error))};
            auto owner=checkpoint_shared_allocation_owner(impl_->world);if(!owner)return {false,{},job.capture_failure(common_failure("CheckpointUntrackedWorldOwner",{},ErrorKind::internal).error)};
            census.owner(owner);auto checked=retain_world_checkpoint_graph(*impl_->world,census);if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};expected=impl_->world->common.state().identity;
        }
        auto retained=job.reserve(CheckpointJobUse::candidate,0);if(!retained.ok)return {false,{},std::move(retained.error)};
        auto lease=checkpoint_make_shared<WorldRestoreRetention>(WorldRestoreRetention::ConstructionKey{},std::move(retained.value),impl_->world,std::move(expected));
        impl_->active_storage_restore=lease;std::shared_ptr<const WorldRestoreRetention> result=std::move(lease);return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
// 原宿主串行写入口复用此实际lease寿命，不新增持久状态或可外部设置的busy标志。
bool ResidentWorld::storage_restore_pending()const noexcept{return !impl_->active_storage_restore.expired();}
}
