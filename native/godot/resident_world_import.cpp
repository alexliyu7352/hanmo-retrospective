// Prepare与Accept各完整读一次相同原件；本机准入先durable，最后只安装一个私有candidate。
#include "resident_world_import.hpp"
#include "resident_profile_coordinator.hpp"
#include "resident_slot_envelope.hpp"
#include "resident/kernel/world_restore_retention.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <sys/random.h>
#include <cstring>
#include <cerrno>
namespace hanmo::resident::storage {
namespace {
bool same_world(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_preview(const WorldImportPreviewIdentity&a,const WorldImportPreviewIdentity&b){return a.instance==b.instance&&a.operation==b.operation&&a.source_bytes_sha256==b.source_bytes_sha256&&same_world(a.snapshot,b.snapshot);}
bool less_fingerprint(const EntryFingerprint&a,const EntryFingerprint&b){return a.key.source_world_id<b.key.source_world_id||(a.key.source_world_id==b.key.source_world_id&&a.key.delivery_id<b.key.delivery_id);}
StableId mint_import_id(){std::uint8_t bytes[32];std::size_t at=0;while(at<sizeof(bytes)){auto n=::getrandom(bytes+at,sizeof(bytes)-at,0);if(n<0){if(errno==EINTR)continue;throw SlotMediaError("AdmissionIdEntropyUnavailable",errno);}if(!n)throw SlotMediaError("AdmissionIdEntropyUnavailable");at+=std::size_t(n);}char value[74];std::memcpy(value,"admission.",10);constexpr char hex[]="0123456789abcdef";for(std::size_t i=0;i<32;++i){value[10+2*i]=hex[bytes[i]>>4];value[11+2*i]=hex[bytes[i]&15];}return StableId(value,sizeof(value));}
// Only the original Accept path calls this after PendingImport.record_memory has
// reserved all fresh record fields. Reuse the one capsule emitter without charging
// a second source-retention allocation for that still-owned record.
CheckpointJobResult<CheckpointJobBytes> encode_owned_import_capsule(const SlotAdmissionCapsule& capsule,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    auto measured=write_slot_admission_capsule_to(nullptr,capsule,capacity,slot,job);if(!measured.ok)return {false,{},std::move(measured.error)};
    auto bytes=CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,measured.value);if(!bytes.ok)return bytes;
    struct Sink final:CheckpointWriteSink {
        CheckpointJobBytes& bytes;std::size_t position=0;explicit Sink(CheckpointJobBytes& value):bytes(value){}
        Status write(const std::uint8_t* input,std::size_t size)override{auto status=bytes.write(position,input,size);if(!status.ok)throw checkpoint_job_detail::FailureException{std::move(status.error)};position+=size;return common_success();}
    } sink(bytes.value);
    auto written=write_slot_admission_capsule_to(&sink,capsule,capacity,slot,job);if(!written.ok)return {false,{},std::move(written.error)};
    if(written.value!=measured.value||sink.position!=measured.value)return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::write_range)};
    return bytes;
}

}
class WorldImportAnalysisResult final:public StorageStreamAnalysisResult {
    friend class WorldImportAnalysis;friend class ResidentProfileCoordinator;
    mutable std::shared_ptr<const WorldRestoreRetention> retention_;
    mutable WorldRestoreCandidate candidate_;
    std::shared_ptr<const WorldImportPreview> preview_;
    bool accepting_=false;mutable bool consumed_=false;
};
class WorldImportAnalysis final:public StorageStreamAnalysis {
public:
    WorldImportAnalysis(std::shared_ptr<const WorldRestoreRetention> retention,std::shared_ptr<const PreparedCatalog> catalog,CheckpointDigest digest,ProfilePurpose purpose,std::shared_ptr<const WorldImportPreview> previous={})
        :retention_(std::move(retention)),catalog_(std::move(catalog)),digest_(digest),purpose_(purpose),previous_(std::move(previous)){}
    CheckpointJobResult<std::shared_ptr<const StorageStreamAnalysisResult>> analyze(const SlotKey& key,const SlotStreamBankPair* banks,SlotReadLease* source,CheckpointJobBudget& job,const SlotCancellation& cancel)const override{
        try{if(!source||banks||key.kind!=SlotFileKind::profile_metadata||source->full_sha256()!=digest_)return {false,{},job.capture_boundary_failure("ImportSourceChanged",{},ErrorKind::rule)};if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            const auto& capacity=engineering_checkpoint_capacity_profile();const auto& limits=engineering_slot_capacity_profile();auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
            auto inspected=inspect_import_slot_envelope(*source,capacity,limits,job);if(!inspected.ok)return {false,{},std::move(inspected.error)};const auto& index=inspected.value.value();const auto& c=index.payloads[1];SlotPayloadReadSource checkpoint(*source,index.payloads[0]);BoundContinuationSource continuation{c.present?source:nullptr,c.offset,c.length,c.sha256};
            auto candidate=WorldRestoreCandidate::prepare(checkpoint,index.payloads[0].sha256,catalog_,continuation,capacity,limits,job);if(!candidate.ok)return {false,{},std::move(candidate.error)};const auto& a=index.payloads[2];auto capsule=decode_slot_admission_capsule(*source,a.offset,a.length,a.sha256,capacity,limits,job);if(!capsule.ok)return {false,{},std::move(capsule.error)};
            // 外来uid/Admission ID/历史同意不搬入本机；purpose亦不能靠改目标参数升级。
            if(capsule.value.value().name_space.purpose!=purpose_)return {false,{},job.capture_boundary_failure("ImportPurposeMismatch",{},ErrorKind::rule)};
            auto original=candidate.value.verify_admission_baseline(capsule.value.value().source_world_id,capsule.value.value().baseline,capacity,job);if(!original.ok)return {false,{},std::move(original.error)};
            auto projection=candidate.value.capture_import_deliveries(capacity,job);if(!projection.ok)return {false,{},std::move(projection.error)};
            auto preview=checkpoint_make_shared<WorldImportPreview>();preview->memory_=CheckpointMemoryCharge(0);preview->baseline_.reserve(projection.value.entries().size());for(const auto&r:projection.value.entries())preview->baseline_.push_back(r.fingerprint);std::sort(preview->baseline_.begin(),preview->baseline_.end(),less_fingerprint);preview->projection_=std::move(projection.value);preview->source_sha256_=digest_;preview->checkpoint_sha256_=index.payloads[0].sha256;
            if(previous_){if(!same_world(previous_->snapshot_identity(),preview->snapshot_identity())||previous_->source_sha256_!=digest_||previous_->checkpoint_sha256_!=preview->checkpoint_sha256_||previous_->baseline_.size()!=preview->baseline_.size())return {false,{},job.capture_boundary_failure("ImportPreviewChanged",{},ErrorKind::rule)};for(std::size_t i=0;i<preview->baseline_.size();++i)if(!same_entry_fingerprint(previous_->baseline_[i],preview->baseline_[i]))return {false,{},job.capture_boundary_failure("ImportPreviewChanged",{},ErrorKind::rule)};}
            if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};auto result=checkpoint_make_shared<WorldImportAnalysisResult>();result->preview_=std::move(preview);result->accepting_=bool(previous_);if(previous_){result->retention_=retention_;result->candidate_=std::move(candidate.value);}std::shared_ptr<const StorageStreamAnalysisResult> owned=std::move(result);return {true,std::move(owned),{}};
        }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
    }
private:std::shared_ptr<const WorldRestoreRetention> retention_;std::shared_ptr<const PreparedCatalog> catalog_;CheckpointDigest digest_;ProfilePurpose purpose_;std::shared_ptr<const WorldImportPreview> previous_;
};
struct ResidentProfileCoordinator::PendingImport {
    CheckpointMemoryCharge choice_memory,record_memory;
    std::shared_ptr<CheckpointJobFailure> failure;
    std::shared_ptr<CheckpointJobBudget> job;
    std::shared_ptr<const WorldRestoreRetention> retention;
    std::shared_ptr<const PreparedCatalog> catalog;
    StorageOperationRef transport;std::optional<WorldIdentity> expected_active;
    WorldImportStatus status;
    ImportHistoryChoice choice;std::optional<AdmissionRecord> record;
    std::shared_ptr<const WorldImportAnalysisResult> accepted;
    JobAdmissionCapsule capsule;
    WorldInstallCancellation cancellation;
    bool confirmed_media_failure=false,retrying_after_install=false,metadata_started=false;
    Integer expected_profile_generation=0;
};
bool ResidentProfileCoordinator::import_analysis_pending()const noexcept{return pending_import_&&(pending_import_->status.phase==WorldImportPhase::preparing||pending_import_->status.phase==WorldImportPhase::revalidating);}
WorldImportStatus ResidentProfileCoordinator::import_status()const{if(!owner_thread()){WorldImportStatus result;result.phase=WorldImportPhase::failed;result.code="ProfileOwnerThreadRequired";return result;}if(!pending_import_){WorldImportStatus result;result.phase=WorldImportPhase::failed;result.code="WorldImportNotPending";return result;}return pending_import_->status;}
StorageAccepted ResidentProfileCoordinator::prepare_world_import(const BoundaryValue& catalog,const StableId& scenario,const CatalogIdentity& expected_catalog,const std::string& path,const CheckpointDigest& digest){
    if(!owner_thread())return {false,{},"ProfileOwnerThreadRequired"};if(closing_||operation_||pending_creation_||pending_import_)return {false,{},"ProfileOperationPending"};if(!loaded_)return {false,{},"ProfileStoreNotLoaded"};
    try{auto made=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());if(!made.ok){return StorageAccepted::rejected(std::move(made.error));}auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);
        auto pending=checkpoint_make_shared<PendingImport>();pending->job=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));pending->failure=checkpoint_make_shared<CheckpointJobFailure>();
        auto retained=session_.retain_storage_world(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*pending->job);if(!retained.ok)return StorageAccepted::rejected(std::move(retained.error));pending->retention=std::move(retained.value);pending->expected_active=pending->retention->expected_identity();
        auto prepared=session_.prepare_storage_catalog(catalog,scenario,expected_catalog,*pending->job);if(!prepared.ok)return StorageAccepted::rejected(std::move(prepared.error));pending->catalog=std::move(prepared.value);pending->expected_profile_generation=selected_.snapshot.generation;
        auto analysis=checkpoint_make_shared<WorldImportAnalysis>(pending->retention,pending->catalog,digest,host_namespace_.purpose);auto accepted=storage_.analyze_import_file(path,digest,pending->expected_active.value_or(WorldIdentity{}),pending->job,std::move(analysis));if(!accepted.ok)return accepted;pending->transport=accepted.reference;pending->status.identity.instance=accepted.reference.instance;pending->status.identity.operation=accepted.reference.operation;pending->status.identity.source_bytes_sha256=digest;operation_=accepted.reference;pending_import_=std::move(pending);return accepted;
    }catch(checkpoint_job_detail::FailureException& e){return StorageAccepted::rejected(std::move(e.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
Status ResidentProfileCoordinator::accept_world_import(const WorldImportPreviewIdentity& identity,const ImportHistoryChoice& choice){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_||!same_preview(identity,pending_import_->status.identity)||pending_import_->status.phase!=WorldImportPhase::preview_ready)return profile_admission_failure("StaleImportPreview");auto& p=*pending_import_;if(closing_||!p.status.preview||selected_.snapshot.generation!=p.expected_profile_generation)return profile_admission_failure("ProfileGenerationConflict");
    try{if(choice.card_history.size()>16384||choice.actor_unlock_history.size()>16384||choice.card_history.size()>16384-choice.actor_unlock_history.size())return profile_admission_failure("AdmissionBaselineLimit");
        for(unsigned kind=0;kind<2;++kind){const auto& selected=kind?choice.actor_unlock_history:choice.card_history;for(std::size_t i=0;i<selected.size();++i){const auto& fingerprint=selected[i];const ResolvedProfileDelivery* row=nullptr;for(const auto& actual:p.status.preview->history())if(same_entry_fingerprint(actual.fingerprint,fingerprint)){row=&actual;break;}if(!row||row->entry.kind!=(kind?ProfileDeliveryKind::actor_unlock:ProfileDeliveryKind::card_discovery))return profile_admission_failure("ImportHistorySelectionMismatch");for(std::size_t j=0;j<i;++j)if(same_delivery_key(selected[j].key,fingerprint.key))return profile_admission_failure("DuplicateImportHistorySelection");}}
        auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::candidate);if(!resource.ok){p.status.failure=std::move(resource.error);return profile_admission_failure("CheckpointJobFailed");}ScopedCheckpointMemory scope(resource.value);auto charge=CheckpointMemoryCharge(0);ImportHistoryChoice frozen{std::vector<EntryFingerprint>(choice.card_history),std::vector<EntryFingerprint>(choice.actor_unlock_history)};
        auto analysis=checkpoint_make_shared<WorldImportAnalysis>(p.retention,p.catalog,identity.source_bytes_sha256,host_namespace_.purpose,p.status.preview);auto queued=storage_.revalidate_import_file(p.transport,std::move(analysis));if(!queued.ok)return profile_admission_failure(queued.code.c_str());p.choice=std::move(frozen);p.choice_memory=std::move(charge);p.status.phase=WorldImportPhase::revalidating;return common_success();
    }catch(checkpoint_job_detail::FailureException& error){p.status.failure=std::move(error.error);return profile_admission_failure("CheckpointJobFailed");}catch(const std::bad_alloc&){return profile_admission_failure("ProfileAdmissionResourceExhausted");}
}
Status ResidentProfileCoordinator::advance_world_import(){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_)return profile_admission_failure("WorldImportNotPending");auto& p=*pending_import_;
    auto fail=[&](const char* code){p.status.phase=WorldImportPhase::failed;p.status.code=code;
        // 纯分析终态可立即释放；有tmp/unknown的原worker记录仍交其真实关闭/恢复路径。
        if(operation_&&!p.metadata_started){auto current=storage_.poll(*operation_);if(current.ok&&(current.state==StorageState::completed||current.state==StorageState::failed||current.state==StorageState::cancelled)){auto released=storage_.release(*operation_);if(released.ok)operation_.reset();}}
        return profile_admission_failure(code);};
    try{
        if(p.status.phase==WorldImportPhase::cancelling){
            if(!operation_){p.status.phase=WorldImportPhase::cancelled;return common_success();}
            if(p.metadata_started){auto progress=advance_metadata();if(operation_){if(status_.phase==ProfileAdmissionPhase::awaiting_recovery){p.status.phase=WorldImportPhase::awaiting_recovery;}return progress;}p.status.admission_stored=status_.phase==ProfileAdmissionPhase::stored&&status_.durability==SlotDurability::durable_confirmed;p.status.phase=WorldImportPhase::cancelled;return common_success();}
            auto current=storage_.poll(*operation_);if(!current.ok)return profile_admission_failure(current.code.c_str());if(current.state==StorageState::queued||current.state==StorageState::running||current.state==StorageState::awaiting_owner_validation)return common_success();auto released=storage_.release(*operation_);if(!released.ok)return profile_admission_failure(released.code.c_str());operation_.reset();p.status.phase=WorldImportPhase::cancelled;return common_success();
        }
        if(import_analysis_pending()){
            auto result=storage_.poll(p.transport);if(!result.ok)return profile_admission_failure(result.code.c_str());if(result.state==StorageState::queued||result.state==StorageState::running)return common_success();
            if(result.state!=StorageState::completed){p.status.phase=result.state==StorageState::cancelled?WorldImportPhase::cancelled:WorldImportPhase::failed;p.status.code=result.code;p.status.failure=result.job_failure;auto released=storage_.release(p.transport);if(released.ok)operation_.reset();return profile_admission_failure(result.code.empty()?"WorldImportFailed":result.code.c_str());}
            auto parsed=std::dynamic_pointer_cast<const WorldImportAnalysisResult>(result.stream_analysis);if(!parsed||parsed->consumed_||!parsed->preview_)return fail("InvalidImportAnalysisResult");
            if(p.status.phase==WorldImportPhase::preparing){if(parsed->accepting_)return fail("InvalidImportAnalysisResult");p.status.preview=parsed->preview_;p.status.identity.snapshot=parsed->preview_->snapshot_identity();p.status.phase=WorldImportPhase::preview_ready;return common_success();}
            if(!parsed->accepting_||!parsed->candidate_.validated())return fail("InvalidImportAnalysisResult");
            // 接纳只前进一次；先持私有candidate并释放已结束的文件分析，后续失败不能重铸Admission。
            p.accepted=parsed;p.status.phase=WorldImportPhase::preparing_admission;
            auto released=storage_.release(p.transport);if(!released.ok)return fail(released.code.c_str());operation_.reset();
            auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::candidate);if(!resource.ok){*p.failure=std::move(resource.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);return fail("CheckpointJobFailed");}ScopedCheckpointMemory scope(resource.value);
            p.record_memory=CheckpointMemoryCharge(0);
            AdmissionRecord record;record.origin=AdmissionOrigin::imported;record.capsule.admission_id=mint_import_id();record.capsule.name_space=ProfileNamespace{StableId(host_namespace_.profile_uid),host_namespace_.purpose,host_namespace_.preview_session_id?std::optional<StableId>(StableId(*host_namespace_.preview_session_id)):std::nullopt};record.capsule.source_world_id=parsed->preview_->snapshot_identity().world_id;record.capsule.origin_snapshot_sha256=parsed->preview_->checkpoint_sha256_;record.capsule.baseline=std::vector<EntryFingerprint>(parsed->preview_->baseline_);auto baseline=admission_baseline_digest(record.capsule.source_world_id,record.capsule.baseline,limits_.admission);if(!baseline.ok)return fail(baseline.error.original.code.c_str());record.capsule.baseline_sha256=baseline.value;record.allowed_history.reserve(p.choice.card_history.size()+p.choice.actor_unlock_history.size());for(const auto* rows:{&p.choice.card_history,&p.choice.actor_unlock_history})for(const auto& e:*rows)record.allowed_history.push_back(e);std::sort(record.allowed_history.begin(),record.allowed_history.end(),less_fingerprint);
            auto valid=validate_admission_record(record,limits_.admission);if(!valid.ok)return fail(valid.error.original.code.c_str());auto encoded=encode_owned_import_capsule(record.capsule,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*p.job);if(!encoded.ok){*p.failure=std::move(encoded.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);return fail("CheckpointJobFailed");}auto capsule=decode_slot_admission_capsule(encoded.value,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*p.job);if(!capsule.ok){*p.failure=std::move(capsule.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);return fail("CheckpointJobFailed");}p.capsule=std::move(capsule.value);p.record=std::move(record);
            auto next=merge_import_admission(selected_.snapshot,*p.record,p.expected_profile_generation,parsed->preview_->projection_,limits_);if(!next.ok)return fail(next.error.original.code.c_str());p.status.phase=WorldImportPhase::persisting_admission;p.metadata_started=true;metadata_job_=p.job;metadata_resource_=resource.value;
            auto begun=begin_profile_merge_candidate(std::move(next.value),parsed->preview_->snapshot_identity(),p.record->capsule.admission_id);if(!begun.ok&&!operation_)return fail(begun.error.original.code.c_str());return begun;
        }
        if(p.status.phase==WorldImportPhase::persisting_admission||p.status.phase==WorldImportPhase::awaiting_recovery){auto progressed=advance_metadata();if(status_.phase==ProfileAdmissionPhase::awaiting_recovery){p.status.phase=WorldImportPhase::awaiting_recovery;return progressed;}if(operation_)return progressed;
            if(status_.phase==ProfileAdmissionPhase::stored&&status_.durability==SlotDurability::durable_confirmed){p.status.admission_stored=true;p.status.profile_sync_blocked=false;
                if(p.cancellation.cancelled()){p.status.phase=WorldImportPhase::cancelled;return common_success();}
                if(p.retrying_after_install){p.retrying_after_install=false;p.status.phase=WorldImportPhase::installed;status_.admission_stored=true;status_.profile_sync_blocked=false;return common_success();}
                p.status.phase=WorldImportPhase::ready_to_install;return install_import_candidate(false);}
            p.status.code=status_.code;p.status.phase=status_.system_error!=0&&status_.durability==SlotDurability::not_published?WorldImportPhase::awaiting_failed_media_confirmation:WorldImportPhase::failed;return progressed;
        }
        if(p.status.phase==WorldImportPhase::rereading_admission)return advance_metadata();
        if(p.status.phase==WorldImportPhase::ready_to_install)return install_import_candidate(false);
        if(p.status.phase==WorldImportPhase::preparing_admission)return fail("ImportAdmissionPreparationInterrupted");
        if(p.status.phase==WorldImportPhase::failed)return profile_admission_failure(p.status.code.empty()?"WorldImportFailed":p.status.code.c_str());return common_success();
    }catch(checkpoint_job_detail::FailureException& e){*p.failure=std::move(e.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);return fail("CheckpointJobFailed");}catch(const std::bad_alloc&){return fail("ProfileAdmissionResourceExhausted");}catch(const SlotMediaError& e){return fail(e.what());}
}
Status ResidentProfileCoordinator::install_import_candidate(bool confirmed){
    auto& p=*pending_import_;if(!p.accepted||p.accepted->consumed_||!p.record||!p.capsule.belongs_to(*p.job))return profile_admission_failure("ValidatedImportCandidateRequired");
    if(confirmed){if(p.status.phase!=WorldImportPhase::awaiting_failed_media_confirmation||!p.confirmed_media_failure)return profile_admission_failure("ImportMediaFailureConfirmationRequired");}else if(!p.status.admission_stored||p.status.phase!=WorldImportPhase::ready_to_install)return profile_admission_failure("ImportAdmissionNotDurable");
    if(closing_||p.cancellation.cancelled())return profile_admission_failure("StorageCancelled");
    auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::candidate);if(!resource.ok){p.status.failure=std::move(resource.error);return profile_admission_failure("CheckpointJobFailed");}ScopedCheckpointMemory scope(resource.value);ProfileAdmissionStatus next;next.world_committed=true;next.admission_stored=!confirmed;next.profile_sync_blocked=confirmed;next.phase=confirmed?ProfileAdmissionPhase::blocked:ProfileAdmissionPhase::stored;next.profile_generation=selected_.snapshot.generation;next.admission_id=p.record->capsule.admission_id;if(confirmed)next.code="ImportAdmissionMediaFailed";
    auto installed=session_.install_storage_candidate(std::move(p.accepted->candidate_),p.expected_active,p.cancellation,*p.job);if(!installed.ok){*p.failure=std::move(installed.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);p.status.phase=WorldImportPhase::failed;return profile_admission_failure("WorldImportInstallRejected");}
    // 从原唯一install起仅noexcept移动/销毁；保原完整baseline与已选历史，以后另存不重建空记录。
    p.accepted->consumed_=true;p.accepted->retention_.reset();p.retention.reset();next.source=installed.value;creation_.reset();creation_identity_.reset();creation_proven_=false;creation_memory_={};active_capsule_.reset();active_capsule_memory_={};active_job_capsule_.reset();active_job_capsule_.emplace(std::move(p.capsule));active_identity_=installed.value;status_.~ProfileAdmissionStatus();new(&status_)ProfileAdmissionStatus(std::move(next));p.status.world_committed=true;p.status.admission_stored=!confirmed;p.status.profile_sync_blocked=confirmed;p.status.phase=WorldImportPhase::installed;return common_success();
}
Status ResidentProfileCoordinator::retry_world_import_admission(const WorldImportPreviewIdentity& identity){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_||!same_preview(identity,pending_import_->status.identity)||!pending_import_->record||!pending_import_->accepted)return profile_admission_failure("OriginalImportReceiptRequired");auto& p=*pending_import_;
    if(closing_||operation_)return profile_admission_failure("ProfileOperationPending");if(!p.status.world_committed||(p.status.phase!=WorldImportPhase::installed&&p.status.phase!=WorldImportPhase::failed&&p.status.phase!=WorldImportPhase::awaiting_failed_media_confirmation)||p.status.admission_stored||!p.status.profile_sync_blocked)return profile_admission_failure("ImportAdmissionRetryNotRequired");
    auto current=session_.identity();if(!current.ok)return {false,{},current.error};if(!active_identity_||current.value.world_id!=active_identity_->world_id||current.value.execution_epoch!=active_identity_->execution_epoch||!active_capsule()||!same_admission_capsule(*active_capsule(),p.record->capsule))return profile_admission_failure("ProfileSnapshotChanged");
    auto accepted=storage_.inspect_profile_metadata({"profile_metadata",SlotFileKind::profile_metadata,namespace_binding_},current.value,p.job);if(!accepted.ok){p.status.failure=accepted.take_failure();return profile_admission_failure(accepted.code.empty()?"CheckpointJobFailed":accepted.code.c_str());}operation_=accepted.reference;reading_=true;p.retrying_after_install=true;p.status.phase=WorldImportPhase::rereading_admission;status_.phase=ProfileAdmissionPhase::reading;return common_success();
}
Status ResidentProfileCoordinator::continue_import_admission_write(){
    if(!pending_import_||pending_import_->status.phase!=WorldImportPhase::rereading_admission)return common_success();auto& p=*pending_import_;
    try{auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::candidate);if(!resource.ok){*p.failure=std::move(resource.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);p.status.phase=WorldImportPhase::failed;return profile_admission_failure("CheckpointJobFailed");}ScopedCheckpointMemory scope(resource.value);
        auto next=resume_import_admission(selected_.snapshot,*p.record,selected_.snapshot.generation,p.accepted->preview_->projection_,limits_);if(!next.ok){p.status.phase=WorldImportPhase::failed;return {false,{},next.error};}p.status.phase=WorldImportPhase::persisting_admission;p.metadata_started=true;metadata_job_=p.job;metadata_resource_=resource.value;return begin_profile_merge_candidate(std::move(next.value),p.accepted->preview_->snapshot_identity(),p.record->capsule.admission_id);
    }catch(checkpoint_job_detail::FailureException& e){*p.failure=std::move(e.error);p.status.failure=CheckpointJobFailureLease::retain(*p.failure);p.status.phase=WorldImportPhase::failed;return profile_admission_failure("CheckpointJobFailed");}
}
Status ResidentProfileCoordinator::confirm_import_without_profile(const WorldImportPreviewIdentity& identity){if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_||!same_preview(identity,pending_import_->status.identity)||pending_import_->status.phase!=WorldImportPhase::awaiting_failed_media_confirmation)return profile_admission_failure("ImportMediaFailureConfirmationRequired");pending_import_->confirmed_media_failure=true;return install_import_candidate(true);}
Status ResidentProfileCoordinator::cancel_world_import(const WorldImportPreviewIdentity& identity){if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_||!same_preview(identity,pending_import_->status.identity))return profile_admission_failure("StaleImportPreview");auto& p=*pending_import_;if(!p.cancellation.request())return profile_admission_failure("WorldImportAlreadyInstalled");if(operation_){auto result=storage_.cancel(*operation_);if(!result.ok&&result.code!="StorageAlreadyCompleted"&&result.code!="StoragePublicationStarted")return profile_admission_failure(result.code.c_str());}p.status.phase=WorldImportPhase::cancelling;return common_success();}
Status ResidentProfileCoordinator::release_world_import(const WorldImportPreviewIdentity& identity){if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!pending_import_||!same_preview(identity,pending_import_->status.identity))return profile_admission_failure("StaleImportPreview");const auto phase=pending_import_->status.phase;if(phase!=WorldImportPhase::installed&&phase!=WorldImportPhase::failed&&phase!=WorldImportPhase::cancelled)return profile_admission_failure("WorldImportPending");if(operation_){auto released=storage_.release(*operation_);if(!released.ok)return profile_admission_failure(released.code.c_str());operation_.reset();}if(pending_import_->status.world_committed&&pending_import_->status.profile_sync_blocked)return profile_admission_failure("ImportAdmissionRetryRequired");pending_import_.reset();return common_success();}
}
