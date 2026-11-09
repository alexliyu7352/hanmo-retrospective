// local_created只由本协调者真正initialize成功铸造；原worker测量原封闭捕获的完整checkpoint。
#include "resident_profile_coordinator.hpp"
#include "resident/kernel/world_checkpoint_capture.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <sys/random.h>
#include <cerrno>
#include <cstring>
namespace hanmo::resident::storage {
namespace {
bool same_created_identity(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
StableId new_local_admission_id(){CheckpointDigest bytes{};std::size_t done=0;while(done<bytes.size()){const auto n=::getrandom(bytes.data()+done,bytes.size()-done,0);if(n<0){if(errno==EINTR)continue;throw SlotMediaError("AdmissionIdEntropyUnavailable",errno);}if(!n)throw SlotMediaError("AdmissionIdEntropyUnavailable");done+=std::size_t(n);}char text[74];std::memcpy(text,"admission.",10);constexpr char digits[]="0123456789abcdef";for(std::size_t n=0;n<bytes.size();++n){text[10+2*n]=digits[bytes[n]>>4];text[11+2*n]=digits[bytes[n]&15];}return StableId(text,sizeof(text));}
class CreationDigestSink final:public CheckpointWriteSink {
public:explicit CreationDigestSink(const SlotCancellation& cancel):cancel_(cancel){}
    Status write(const std::uint8_t*,std::size_t)override{return cancel_.cancelled()?profile_admission_failure("StorageCancelled"):common_success();}
private:const SlotCancellation& cancel_;
};
class CreationProof final:public StorageStreamAnalysisResult {public:WorldIdentity source;CheckpointDigest checkpoint_sha256{};};
class CreationAnalysis final:public StorageStreamAnalysis {
public:explicit CreationAnalysis(std::shared_ptr<const WorldCheckpointCapture> source):source_(std::move(source)){}
    CheckpointJobResult<std::shared_ptr<const StorageStreamAnalysisResult>> analyze(const SlotKey& key,const SlotStreamBankPair* banks,SlotReadLease* staged,CheckpointJobBudget& job,const SlotCancellation& cancel)const override{
        try{if(key.kind!=SlotFileKind::profile_metadata||banks||staged||!source_||!source_->belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
            if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            auto resource=checkpoint_memory_resource(job,CheckpointJobUse::encode);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
            CreationDigestSink sink(cancel);auto digest=source_->write(sink,engineering_checkpoint_capacity_profile(),job);if(!digest.ok)return {false,{},std::move(digest.error)};
            if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            auto proof=checkpoint_make_shared<CreationProof>();proof->source=source_->source_identity();proof->checkpoint_sha256=digest.value.full_checkpoint_sha256;std::shared_ptr<const StorageStreamAnalysisResult> result=std::move(proof);return {true,std::move(result),{}};
        }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
    }
private:std::shared_ptr<const WorldCheckpointCapture> source_;
};
}
struct ResidentProfileCoordinator::PendingCreation {
    std::shared_ptr<CheckpointJobBudget> job;
    std::shared_ptr<const WorldCheckpointCapture> capture;
    CheckpointJobFailure failure;
};
ProfileAdmissionStatus ResidentProfileCoordinator::create_local_world(const BoundaryValue& catalog,const BoundaryValue& request){
    try{
        if(!owner_thread()){auto out=status_;out.code="ProfileOwnerThreadRequired";return out;}
        if(closing_||creation_||active_capsule()||operation_||pending_creation_||pending_import_){auto out=status_;out.code=closing_?"ProfileClosing":"LocalCreationAlreadyStarted";return out;}
        const auto& capacity=engineering_checkpoint_capacity_profile();const auto& slot=engineering_slot_capacity_profile();auto made=CheckpointJobBudget::create(capacity,slot);if(!made.ok){status_.fixed_failure=made.error.code();status_.job_failure=std::move(made.error);status_.phase=ProfileAdmissionPhase::blocked;return status_;}
        auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok){status_.fixed_failure=resource.error.code();status_.job_failure=std::move(resource.error);status_.phase=ProfileAdmissionPhase::blocked;return status_;}ScopedCheckpointMemory scope(resource.value);
        pending_creation_=checkpoint_make_shared<PendingCreation>();pending_creation_->job=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));
        // 随机ID/namespace在原create前准备，scope不从导入文件授予；空baseline是原local_created合同。
        creation_memory_=CheckpointMemoryCharge(0);
        AdmissionRecord record;record.origin=AdmissionOrigin::local_created;record.capsule.admission_id=new_local_admission_id();record.capsule.name_space=ProfileNamespace{StableId(host_namespace_.profile_uid),host_namespace_.purpose,host_namespace_.preview_session_id?std::optional<StableId>(StableId(*host_namespace_.preview_session_id)):std::nullopt};creation_=std::move(record);
        auto created=session_.initialize_native(catalog,request);if(!created.ok){creation_.reset();creation_memory_={};pending_creation_.reset();status_.code=created.error.original.code;status_.phase=ProfileAdmissionPhase::blocked;return status_;}
        status_.world_committed=true;auto identity=session_.identity();if(!identity.ok){block(identity.error.original.code);return status_;}
        creation_identity_=identity.value;status_.source=identity.value;status_.admission_id=creation_->capsule.admission_id;
        auto begun=start_creation_proof();if(!begun.ok)block(begun.error.original.code);return status_;
    }catch(checkpoint_job_detail::FailureException& e){if(pending_creation_){pending_creation_->failure=std::move(e.error);status_.job_failure=CheckpointJobFailureLease::retain(pending_creation_->failure);}else {status_.fixed_failure=e.error.code();status_.job_failure=std::move(e.error);}status_.phase=ProfileAdmissionPhase::blocked;status_.profile_sync_blocked=true;pending_creation_.reset();if(!status_.world_committed){creation_.reset();creation_memory_={};}return status_;}
    catch(const SlotMediaError& error){status_.code=StorageDiagnostic::from_media(error);status_.phase=ProfileAdmissionPhase::blocked;status_.profile_sync_blocked=true;pending_creation_.reset();if(!status_.world_committed){creation_.reset();creation_memory_={};}return status_;}
    catch(const std::bad_alloc&){status_.fixed_failure=CheckpointJobFailureCode::allocation_failed;status_.phase=ProfileAdmissionPhase::blocked;status_.profile_sync_blocked=true;pending_creation_.reset();if(!status_.world_committed){creation_.reset();creation_memory_={};}return status_;}
}
Status ResidentProfileCoordinator::start_creation_proof(){
    if(!creation_||!creation_identity_||!status_.world_committed||creation_->origin!=AdmissionOrigin::local_created)return profile_admission_failure("OriginalLocalCreationReceiptRequired");
    if(creation_proven_)return loaded_?begin_pending_write():begin_read();
    if(operation_)return profile_admission_failure("ProfileOperationPending");
    auto actual=session_.identity();if(!actual.ok)return {false,{},actual.error};if(!same_created_identity(actual.value,*creation_identity_))return profile_admission_failure("OriginalCreationSnapshotUnavailable");
    const auto& capacity=engineering_checkpoint_capacity_profile();const auto& slot=engineering_slot_capacity_profile();
    if(!pending_creation_){auto made=CheckpointJobBudget::create(capacity,slot);if(!made.ok){status_.fixed_failure=made.error.code();status_.job_failure=std::move(made.error);return profile_admission_failure("ProfileCreationResourceFailure");}auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok){status_.fixed_failure=resource.error.code();status_.job_failure=std::move(resource.error);return profile_admission_failure("ProfileCreationResourceFailure");}ScopedCheckpointMemory scope(resource.value);pending_creation_=checkpoint_make_shared<PendingCreation>();pending_creation_->job=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));}
    auto& pending=*pending_creation_;auto fail=[&](CheckpointJobFailure&& error){pending.failure=std::move(error);status_.job_failure=CheckpointJobFailureLease::retain(pending.failure);pending_creation_.reset();return profile_admission_failure("ProfileCreationResourceFailure");};
    auto resource=checkpoint_memory_resource(*pending.job,CheckpointJobUse::encode);if(!resource.ok)return fail(std::move(resource.error));ScopedCheckpointMemory scope(resource.value);
    auto capture=session_.capture_storage_checkpoint(capacity,slot,*pending.job);if(!capture.ok)return fail(std::move(capture.error));if(!same_created_identity(capture.value->source_identity(),*creation_identity_)){pending_creation_.reset();return profile_admission_failure("OriginalCreationSnapshotUnavailable");}pending.capture=std::move(capture.value);
    auto analysis=checkpoint_make_shared<CreationAnalysis>(pending.capture);auto accepted=storage_.analyze_profile_capture({"profile_metadata",SlotFileKind::profile_metadata,namespace_binding_},*creation_identity_,pending.job,std::move(analysis));if(!accepted.ok){status_.job_failure=accepted.take_failure();pending_creation_.reset();return profile_admission_failure(accepted.code.empty()?"ProfileCreationResourceFailure":accepted.code.c_str());}
    operation_=accepted.reference;reading_=false;status_.phase=ProfileAdmissionPhase::proving_creation;status_.code.clear();return common_success();
}
Status ResidentProfileCoordinator::advance_creation_proof(){
    if(!pending_creation_||!operation_)return profile_admission_failure("ProfileOperationNotPending");
    auto pending=pending_creation_;
    try{
        auto result=storage_.poll(*operation_);if(!result.ok)return block(result.code);
        if(result.state==StorageState::queued||result.state==StorageState::running)return common_success();
        if(result.state!=StorageState::completed){if(result.state==StorageState::failed||result.state==StorageState::cancelled||result.state==StorageState::closed_unconfirmed){status_.job_failure=result.job_failure;auto released=storage_.release(*operation_);if(!released.ok)return block(released.code);operation_.reset();pending_creation_.reset();return block(result.code.empty()?"ProfileCreationProofFailed":result.code);}return block("ProfileCreationInvalidPhase");}
        auto proof=std::dynamic_pointer_cast<const CreationProof>(result.stream_analysis);
        // 结果只持拥有值、无fd；先释放终态票据，后续候选/诊断分配失败也不留下不可释放请求。
        auto released=storage_.release(*operation_);if(!released.ok)return block(released.code);operation_.reset();pending_creation_.reset();
        if(!proof||!creation_||!creation_identity_||!same_created_identity(proof->source,*creation_identity_))return block("OriginalCreationSnapshotUnavailable");
        auto resource=checkpoint_memory_resource(*pending->job,CheckpointJobUse::candidate);if(!resource.ok){pending->failure=std::move(resource.error);status_.job_failure=CheckpointJobFailureLease::retain(pending->failure);return block("ProfileCreationResourceFailure");}ScopedCheckpointMemory scope(resource.value);
        creation_->capsule.source_world_id=proof->source.world_id;creation_->capsule.origin_snapshot_sha256=proof->checkpoint_sha256;
        auto digest=admission_baseline_digest(proof->source.world_id,{},limits_.admission);if(!digest.ok)return block(digest.error.original.code);creation_->capsule.baseline_sha256=digest.value;auto valid=validate_admission_record(*creation_,limits_.admission);if(!valid.ok)return block(valid.error.original.code);
        auto charge=CheckpointMemoryCharge(0);SlotAdmissionCapsule capsule=creation_->capsule;
        metadata_job_=pending->job;metadata_resource_=resource.value;active_capsule_=std::move(capsule);active_capsule_memory_=std::move(charge);active_identity_=*creation_identity_;creation_proven_=true;
        return loaded_?begin_pending_write():begin_read();
    }catch(checkpoint_job_detail::FailureException& e){pending->failure=std::move(e.error);status_.job_failure=CheckpointJobFailureLease::retain(pending->failure);if(!operation_)pending_creation_.reset();return block("ProfileCreationResourceFailure");}
    catch(const std::bad_alloc&){status_.fixed_failure=CheckpointJobFailureCode::allocation_failed;if(!operation_)pending_creation_.reset();return block("ProfileCreationResourceFailure");}
}
}
