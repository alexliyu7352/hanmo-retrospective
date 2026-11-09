// 自动投递与显式历史授权都从同一session重取原已提交S4投影，写同一META代。
#include "resident_profile_coordinator.hpp"
#include "resident_profile_copy.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
namespace hanmo::resident::storage {
struct ResidentProfileCoordinator::ProfileTransactionOwner {
    CheckpointJobReservation retained_inputs;
    std::shared_ptr<CheckpointJobBudget> job;
    CheckpointJobFailure failure;
};
void ResidentProfileCoordinator::release_profile_transaction_inputs()noexcept{if(profile_transaction_)profile_transaction_->retained_inputs={};}
std::shared_ptr<const ProfileDeliveryReceipt> ResidentProfileCoordinator::delivery_receipt()const noexcept{
    return owner_thread()?delivered_receipt_:nullptr;
}
Status ResidentProfileCoordinator::begin_profile_merge_candidate(ProfileMergeCandidate&& candidate,const WorldIdentity& source,const StableId& id){
    const auto* record=find_profile_admission(candidate.snapshot,id);if(!record)return profile_admission_failure("MissingLocalAdmissionRecord");
    auto receipt=checkpoint_make_shared<ProfileDeliveryReceipt>();receipt->committed_projection_identity=source;receipt->profile_generation=candidate.snapshot.generation;receipt->record_generation=record->record_generation;
    receipt->owned_memory=std::move(candidate.result_memory);receipt->accepted_delivery_ids=std::move(candidate.accepted_delivery_ids);receipt->already_present_ids=std::move(candidate.already_present_ids);receipt->historical_quarantined_ids=std::move(candidate.historical_quarantined_ids);
    pending_delivery_receipt_=std::move(receipt);delivered_receipt_.reset();
    auto begun=begin_metadata_candidate(std::move(candidate.snapshot),source,id);if(!begun.ok&&!operation_)pending_delivery_receipt_.reset();return begun;
}
Status ResidentProfileCoordinator::begin_profile_transaction(bool grant,const StableId& id,Integer record_generation,Integer generation,const ImportHistoryChoice* choice,const std::vector<EntryFingerprint>* legacy){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");
    if(!active_capsule()||!active_identity_||!loaded_||closing_||operation_||pending_import_||pending_creation_)return profile_admission_failure("ProfileMergeNotReady");
    if(active_capsule()->admission_id!=id)return profile_admission_failure("AdmissionIdentityConflict");
    const auto* record=find_profile_admission(selected_.snapshot,id);if(!record||!same_admission_capsule(record->capsule,*active_capsule()))return profile_admission_failure("AdmissionRecoveryRequired");
    if(generation!=selected_.snapshot.generation)return profile_admission_failure("ProfileGenerationConflict");if(record_generation!=record->record_generation)return profile_admission_failure("AdmissionGenerationConflict");
    // 自动并集和显式历史授权都不能绕过活动世界的同步阻断。
    if(status_.profile_sync_blocked)return profile_admission_failure("ProfileSyncBlocked");
    try{
        auto made=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());if(!made.ok){status_.fixed_failure=made.error.code();status_.job_failure=std::move(made.error);return block("ProfileAdmissionResourceExhausted");}
        auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok){status_.fixed_failure=resource.error.code();status_.job_failure=std::move(resource.error);return block("ProfileAdmissionResourceExhausted");}ScopedCheckpointMemory scope(resource.value);
        auto pending=checkpoint_make_shared<ProfileTransactionOwner>();pending->job=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));
        {auto scratch=checkpoint_memory_resource(*pending->job,CheckpointJobUse::scratch);if(!scratch.ok){status_.fixed_failure=scratch.error.code();status_.job_failure=std::move(scratch.error);return block("ProfileAdmissionResourceExhausted");}
            CheckpointRetentionCensus census(scratch.value,CheckpointJobUse::candidate);retain_profile_metadata(selected_.snapshot,census);
            // Both bank buffers and their actual shared owner were created by the original worker/prepared-bank factory.
            if(!banks_ownership_)return profile_admission_failure("ProfileBankAllocationOwnerRequired",{},ErrorKind::internal);
            census.owner(checkpoint_shared_allocation_owner(banks_ownership_));banks_ownership_->retain(census);
            auto retained=pending->job->reserve(CheckpointJobUse::candidate,0);if(!retained.ok){pending->failure=std::move(retained.error);profile_transaction_=std::move(pending);status_.job_failure=CheckpointJobFailureLease::retain(profile_transaction_->failure);return block("ProfileAdmissionResourceExhausted");}pending->retained_inputs=std::move(retained.value);}
        profile_transaction_=std::move(pending);metadata_job_=profile_transaction_->job;metadata_resource_=resource.value;
        bool submitted=false;struct InputGuard{CheckpointJobReservation& value;bool& submitted;~InputGuard()noexcept{if(!submitted)value={};}} input_guard{profile_transaction_->retained_inputs,submitted};
        auto captured=session_.capture_profile_deliveries(engineering_checkpoint_capacity_profile(),*profile_transaction_->job);
        if(!captured.ok){profile_transaction_->failure=std::move(captured.error);status_.job_failure=CheckpointJobFailureLease::retain(profile_transaction_->failure);return block("ProfileProjectionFailed");}
        const auto& source=captured.value.source();if(source.world_id!=active_identity_->world_id||source.execution_epoch!=active_identity_->execution_epoch)return block("ProfileSnapshotChanged");
        CheckpointMemoryCharge legacy_memory;ImportHistoryChoice separated;
        if(legacy){if(legacy->size()>admission_fingerprint_limit)return profile_admission_failure("AdmissionBaselineLimit");reserve_profile_rows(separated.card_history,legacy->size(),legacy_memory);reserve_profile_rows(separated.actor_unlock_history,legacy->size(),legacy_memory);
            for(const auto& fingerprint:*legacy){const ResolvedProfileDelivery* actual=nullptr;for(const auto& row:captured.value.entries())if(same_entry_fingerprint(row.fingerprint,fingerprint)){actual=&row;break;}if(!actual)return profile_admission_failure("ImportHistorySelectionMismatch");(legacy_memory).grow(0);(actual->entry.kind==ProfileDeliveryKind::actor_unlock?separated.actor_unlock_history:separated.card_history).push_back(fingerprint);}choice=&separated;
        }
        auto next=grant?grant_imported_history(selected_.snapshot,id,record_generation,generation,choice->card_history,choice->actor_unlock_history,captured.value,limits_):merge_profile_deliveries(selected_.snapshot,id,record_generation,generation,captured.value,limits_);
        if(!next.ok)return {false,{},next.error};auto begun=begin_profile_merge_candidate(std::move(next.value),source,id);submitted=operation_.has_value();return begun;
    }catch(checkpoint_job_detail::FailureException& e){if(profile_transaction_){profile_transaction_->failure=std::move(e.error);status_.job_failure=CheckpointJobFailureLease::retain(profile_transaction_->failure);}else {status_.fixed_failure=e.error.code();status_.job_failure=std::move(e.error);}return block("ProfileAdmissionResourceExhausted");}
    catch(const std::bad_alloc&){status_.fixed_failure=CheckpointJobFailureCode::allocation_failed;return block("ProfileAdmissionResourceExhausted");}
}
Status ResidentProfileCoordinator::begin_merge(Integer record_generation,Integer generation){if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!active_capsule())return profile_admission_failure("AdmissionRecoveryRequired");return begin_profile_transaction(false,active_capsule()->admission_id,record_generation,generation,nullptr);}
Status ResidentProfileCoordinator::begin_grant_imported_history(const StableId& id,Integer record_generation,Integer generation,const ImportHistoryChoice& choice){return begin_profile_transaction(true,id,record_generation,generation,&choice);}
Status ResidentProfileCoordinator::begin_authorize_history(const StableId& id,const std::vector<EntryFingerprint>& requested,Integer generation){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");const auto* record=find_profile_admission(selected_.snapshot,id);if(!record)return profile_admission_failure("MissingLocalAdmissionRecord");return begin_profile_transaction(true,id,record->record_generation,generation,nullptr,&requested);
}
}
