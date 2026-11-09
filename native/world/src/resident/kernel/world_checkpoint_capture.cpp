#include "resident/checkpoint/projection_writer.hpp"
#include "world_checkpoint_capture.hpp"
#include "world_impl.hpp"
#include "world_restore_candidate.hpp"
#include "continuation_binding.hpp"
#include "world_copy_plan.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
WorldCheckpointCapture::WorldCheckpointCapture(ConstructionKey,CheckpointJobReservation&& retained,CheckpointMemoryCharge&& metadata,std::shared_ptr<CheckpointMemoryResource> candidate_memory,std::shared_ptr<const WorldBundle> world,CheckpointIdentity&& identity,WorldIdentity source,CheckpointDomainList&& domains,CapturedContinuation&& continuation)
    :retained_(std::move(retained)),metadata_(std::move(metadata)),candidate_memory_(std::move(candidate_memory)),world_(std::move(world)),identity_(std::move(identity)),source_(source),domains_(std::move(domains)),continuation_(std::move(continuation)){}
const std::shared_ptr<const PreparedCatalog>& WorldCheckpointCapture::catalog()const noexcept{return world_->catalog;}
Status WorldCheckpointCapture::visit_section(const StableId& name,CodecVisitor& c)const{
    if(c.reading())return c.reject("CheckpointSnapshotReadOnly");
    return visit_captured_world_checkpoint_section(*world_,name,c);
}
CheckpointJobResult<CheckpointStreamReceipt> WorldCheckpointCapture::write(CheckpointWriteSink& sink,const CapacityProfile& capacity,CheckpointJobBudget& job)const{
    try{
        if(!belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        auto valid=job.check_profile(capacity);if(!valid.ok)return {false,{},std::move(valid.error)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::encode);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        auto memory=CheckpointMemoryCharge(0);
        std::vector<CheckpointSectionProducer> producers;producers.reserve(domains_.size());
        auto lease=shared_from_this();for(const auto& d:domains_)producers.push_back({StableId(d.name),d.codec_version,lease});
        return encode_checkpoint_v2(identity_,producers,domains_.values(),capacity,sink,job);
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<CheckpointJobBytes> WorldCheckpointCapture::bind_continuation(const CheckpointDigest& digest,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job)const{
    if(!belongs_to(job)||!continuation_.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
    if(!continuation_.value())return CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,0);
    return bind_continuation_checkpoint_v2(*continuation_.value(),digest,capacity,slot,job);
}
std::shared_ptr<const WorldCheckpointCapture> ResidentWorld::active_storage_capture()const noexcept{return impl_->active_checkpoint_capture.lock();}
CheckpointJobResult<std::shared_ptr<const WorldCheckpointCapture>> ResidentWorld::capture_storage_checkpoint(const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{
        if(!impl_->world)return {false,{},job.capture_boundary_failure("NoWorld",{},ErrorKind::rule)};
        if(!impl_->active_checkpoint_capture.expired())return {false,{},job.capture_boundary_failure("StorageCaptureBusy",{},ErrorKind::internal)};
        auto valid=job.check_profile(capacity);if(!valid.ok)return {false,{},std::move(valid.error)};valid=job.check_profile(slot);if(!valid.ok)return {false,{},std::move(valid.error)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::encode);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        // 完整profile/源/原导出额外失败门全在所属线程先核。worker只遍历原私有只读lease。
        ProjectionShapeWriter semantic_values(capacity,StableId{},job);auto checked=impl_->world->validate(semantic_values);if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};
        checked=validate_world_export_shape(*impl_->world,capacity,job);if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};
        auto captured=capture_continuation_for_checkpoint(job);if(!captured.ok)return {false,{},std::move(captured.error)};
        checked=impl_->world->prepare_checkpoint_read_cache();if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};
        auto owner=checkpoint_shared_allocation_owner(impl_->world);if(!owner)return {false,{},job.capture_boundary_failure("CheckpointUntrackedWorldOwner",{},ErrorKind::internal)};
        auto scratch=checkpoint_memory_resource(job,CheckpointJobUse::scratch);if(!scratch.ok)return {false,{},std::move(scratch.error)};
        CheckpointRetentionCensus census(scratch.value,CheckpointJobUse::encode);census.owner(owner);checked=retain_world_checkpoint_graph(*impl_->world,census);if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};
        // 生命周期检查通过后才增加异步共享拥有者；旧/新世界并存仍由原租约持有。
        auto retained=job.reserve(CheckpointJobUse::encode,0);if(!retained.ok)return {false,{},std::move(retained.error)};
        auto metadata=CheckpointMemoryCharge(0);
        const auto source=impl_->world->common.state().identity;CheckpointIdentity identity{source.world_id,source.revision,source.tick,CatalogIdentity(impl_->world->catalog->identity)};
        auto domains=select_world_checkpoint_domains(*impl_->world->catalog);
        auto candidate_memory=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!candidate_memory.ok)return {false,{},std::move(candidate_memory.error)};
        auto lease=checkpoint_make_shared<WorldCheckpointCapture>(WorldCheckpointCapture::ConstructionKey{},std::move(retained.value),std::move(metadata),std::move(candidate_memory.value),impl_->world,std::move(identity),source,std::move(domains),std::move(captured.value));
        impl_->active_checkpoint_capture=lease;std::shared_ptr<const WorldCheckpointCapture> result=std::move(lease);return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}
