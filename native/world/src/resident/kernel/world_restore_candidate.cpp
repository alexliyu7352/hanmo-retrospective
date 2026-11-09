#include "resident/checkpoint/projection_writer.hpp"
#include "world_restore_candidate.hpp"
#include "world_codec.hpp"
#include "native_director_wake_world.hpp"
#include "native_transport_catalog.hpp"
#include <new>
namespace hanmo::resident {
WorldRestoreCandidate& WorldRestoreCandidate::operator=(WorldRestoreCandidate&& other)noexcept{if(this!=&other){this->~WorldRestoreCandidate();new(this)WorldRestoreCandidate(std::move(other));}return *this;}
CheckpointJobResult<WorldRestoreCandidate> WorldRestoreCandidate::prepare(CheckpointReadSource& checkpoint,const CheckpointDigest& digest,std::shared_ptr<const PreparedCatalog> catalog,const BoundContinuationSource& continuation,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{if(!catalog)return {false,{},job.capture_failure(common_failure("NoCatalog").error)};auto checked=job.check_profile(capacity);if(!checked.ok)return {false,{},std::move(checked.error)};checked=job.check_profile(slot);if(!checked.ok)return {false,{},std::move(checked.error)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);CodecDecodeBudget budget(capacity,job);
        auto decoded=decode_world_checkpoint_stream(checkpoint,*catalog,capacity,budget,job,&digest);if(!decoded.ok)return {false,{},std::move(decoded.error)};
        WorldRestoreCandidate result;result.world_=checkpoint_make_shared<WorldBundle>(std::move(catalog),std::move(decoded.value));result.world_->common.state().identity.execution_epoch=0;
        auto hydrated=restore_native_transport_derived_values_in_candidate(*result.world_);if(!hydrated.ok)return {false,{},job.capture_failure(std::move(hydrated.error))};
        auto cache=restore_native_complete_director_wake_cache_in_candidate(*result.world_);if(!cache.ok)return {false,{},job.capture_failure(std::move(cache.error))};
        ProjectionShapeWriter semantic_values(capacity,StableId{},job);auto valid=result.world_->validate(semantic_values);if(!valid.ok)return {false,{},job.capture_failure(std::move(valid.error))};valid=validate_world_export_shape(*result.world_,capacity,job);if(!valid.ok)return {false,{},job.capture_failure(std::move(valid.error))};
        result.memory_=budget.take_job_reservation();
        if(continuation.source){CodecDecodeBudget continuation_budget(capacity,job);auto stopped=checkpoint_make_unique<ContinuationSnapshot>();auto read=decode_bound_continuation_into_candidate(*continuation.source,continuation.begin,continuation.length,digest,continuation.sha256,capacity,slot,continuation_budget,job,*stopped);if(!read.ok)return {false,{},std::move(read.error)};valid=validate_continuation_snapshot(*result.world_,*stopped);if(!valid.ok)return {false,{},job.capture_failure(std::move(valid.error))};result.continuation_=std::move(stopped);result.continuation_memory_=continuation_budget.take_job_reservation();}
        else if(continuation.length)return {false,{},job.capture_failure(common_failure("ContinuationSourceMissing").error)};
        result.source_=result.world_->common.state().identity;result.validated_=true;return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobStatus WorldRestoreCandidate::verify_admission_baseline(const WorldId& source,const std::vector<EntryFingerprint>& baseline,const CapacityProfile& capacity,CheckpointJobBudget& job)const{
    try{auto checked=job.check_profile(capacity);if(!checked.ok)return checked;if(!validated()||!memory_.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        if(source!=source_.world_id)return {false,{},job.capture_failure(common_failure("AdmissionCheckpointWorldMismatch").error)};
        if(baseline.size()>16384)return {false,{},job.capture_failure(common_failure("AdmissionBaselineLimit").error)};
        if(world_->profile_deliveries.entries.empty())return {true,{},{}};
        auto memory=checkpoint_memory_resource(job,CheckpointJobUse::scratch);if(!memory.ok)return {false,{},std::move(memory.error)};ScopedCheckpointMemory scope(memory.value);auto source_read=world_->profile_acquisition_read();if(!source_read.ok)return {false,{},job.capture_failure(std::move(source_read.error))};
        for(const auto& old:baseline){if(old.key.source_world_id!=source)return {false,{},job.capture_failure(common_failure("AdmissionCheckpointWorldMismatch").error)};const ProfileDeliveryEntry* found=nullptr;for(const auto& entry:world_->profile_deliveries.entries)if(entry.delivery_id==old.key.delivery_id){found=&entry;break;}if(!found)continue;
            auto hash=profile_delivery_fingerprint(*found,source_read.value->sources(),source_read.value->acquisitions(),source_read.value->control_sources(),capacity,job);if(!hash.ok)return {false,{},std::move(hash.error)};if(hash.value!=old.entry_sha256)return {false,{},job.capture_failure(common_failure("DeliveryIdentityConflict").error)};
        }return {true,{},{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}

CheckpointJobResult<ValidatedImportDeliveries> WorldRestoreCandidate::capture_import_deliveries(const CapacityProfile& capacity,CheckpointJobBudget& job)const{
    try{auto checked=job.check_profile(capacity);if(!checked.ok)return {false,{},std::move(checked.error)};if(!validated()||!memory_.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        if(world_->profile_deliveries.entries.empty()){ValidatedImportDeliveries empty;empty.source_=source_;empty.validated_=true;return {true,std::move(empty),{}};}
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);auto read=world_->profile_acquisition_read();if(!read.ok)return {false,{},job.capture_failure(std::move(read.error))};
        return ValidatedImportDeliveries::capture(world_->common.state(),world_->profile_deliveries,read.value->sources(),read.value->acquisitions(),read.value->control_sources(),capacity,job);
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}

}
