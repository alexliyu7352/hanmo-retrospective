// 原创建/冷恢复/保存使用同一个session；profile worker只消费冻结元档案字节，不能执行世界。
#include "resident_profile_coordinator.hpp"
#include "resident_profile_copy.hpp"
#include "resident/checkpoint/checkpoint_selection.hpp"
#include "resident/kernel/exact_time_profile.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <random>

namespace hanmo::resident::storage {
namespace {
bool identity_equal(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool catalog_equal(const CatalogIdentity& a,const CatalogIdentity& b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
// 仅借本调用已冻结的实际checkpoint字节；读取不发布第二世界或重放取得。
class BytesSource final : public CheckpointReadSource {
public:
    explicit BytesSource(const Bytes& b):bytes_(b){}
    std::uint64_t size()const override{return bytes_.size();}
    Status read(std::uint64_t offset,std::uint8_t* out,std::size_t size)override{if(offset>bytes_.size()||size>bytes_.size()-offset)return profile_admission_failure("ProfileCheckpointShortRead");if(size)std::memcpy(out,bytes_.data()+offset,size);return {true,{},{}};}
private:const Bytes& bytes_;
};
Outcome<CheckpointStreamIndex> inspect(const Bytes& bytes){BytesSource source(bytes);return inspect_checkpoint_v2_stream(source,checkpoint_domains_from_identity,engineering_checkpoint_capacity_profile());}
// namespace物理绑定只能来自宿主构造参数；任何不匹配配置在启动前拒绝，不采纳胶囊输入。
StorageServiceLimits host_limits(const ProfileNamespace& host,StorageServiceLimits limits){auto binding=profile_namespace_binding(host);if(!binding.ok)throw std::invalid_argument("InvalidProfileNamespace");if(!limits.media.host_profile_namespace_binding.empty()&&limits.media.host_profile_namespace_binding!=binding.value)throw std::invalid_argument("StorageProfileNamespaceMismatch");limits.media.host_profile_namespace_binding=std::move(binding.value);return limits;}
// 随机本机ID不取world seed或导入文件；它只是唯一标识，不声称密码学签名。
StableId local_admission_id(){std::random_device random;CheckpointDigest bytes{};for(auto& b:bytes)b=static_cast<std::uint8_t>(random());return "admission."+profile_digest_hex(bytes);}
}
ResidentProfileCoordinator::ResidentProfileCoordinator(ResidentReplaySession& session,ProfileNamespace host,std::string directory,ProfileStoreLimits limits,StorageServiceLimits media
#ifdef HANMO_STORAGE_TEST_FAULTS
    ,SlotIoTestHooks hooks
#endif
):session_(session),host_namespace_(std::move(host)),namespace_binding_(profile_namespace_binding(host_namespace_).value),limits_(limits),storage_(std::move(directory),host_limits(host_namespace_,std::move(media))
#ifdef HANMO_STORAGE_TEST_FAULTS
    ,std::move(hooks)
#endif
),owner_(std::this_thread::get_id()){}
ResidentProfileCoordinator::~ResidentProfileCoordinator(){storage_.request_close();}
bool ResidentProfileCoordinator::owner_thread()const noexcept{return owner_==std::this_thread::get_id();}
Status ResidentProfileCoordinator::block(const StorageDiagnostic& code){status_.code=code;status_.phase=ProfileAdmissionPhase::blocked;status_.profile_sync_blocked=true;return profile_admission_failure(code.c_str());}
ProfileAdmissionStatus ResidentProfileCoordinator::status()const{return status_;}
Status ResidentProfileCoordinator::begin_read(){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");
    if(closing_)return profile_admission_failure("ProfileClosing");
    if(operation_)return profile_admission_failure("ProfileOperationPending");
    // 冷恢复关联后不能用普通重读悄悄更换已准入快照；仅持原创建回执者可走immutable补记录。
    if(active_capsule()&&!creation_)return profile_admission_failure("ProfileReadRequiresUnboundSession");
    if(!metadata_job_){auto made=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());if(!made.ok){status_.fixed_failure=made.error.code();status_.job_failure=std::move(made.error);return block("CheckpointJobFailed");}auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok){status_.fixed_failure=resource.error.code();status_.job_failure=std::move(resource.error);return block("CheckpointJobFailed");}ScopedCheckpointMemory scope(resource.value);metadata_job_=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));metadata_resource_=resource.value;}
    auto source=session_.identity();auto accepted=metadata_job_?storage_.inspect_profile_metadata({"profile_metadata",SlotFileKind::profile_metadata,namespace_binding_},source.ok?source.value:WorldIdentity{},metadata_job_):storage_.inspect({"profile_metadata",SlotFileKind::profile_metadata,namespace_binding_},source.ok?source.value:WorldIdentity{});if(!accepted.ok){status_.job_failure=accepted.take_failure();return block(accepted.code.empty()?"CheckpointJobFailed":accepted.code);}operation_=accepted.reference;reading_=true;loaded_=false;status_.phase=ProfileAdmissionPhase::reading;status_.code.clear();return {true,{},{}};
}
Status ResidentProfileCoordinator::begin_pending_write(){
    if(!creation_||!creation_proven_||!creation_identity_)return block("OriginalLocalCreationReceiptRequired");
    if(!loaded_||!banks_)return block("ProfileStoreNotLoaded");
    if(operation_)return profile_admission_failure("ProfileOperationPending");
    // 刷新实际本机文件后若原record已缺失/不符，旧durable状态不能带入待补阶段。
    const auto* existing=find_profile_admission(selected_.snapshot,creation_->capsule.admission_id);
    if(!existing||!same_admission_capsule(existing->capsule,creation_->capsule)||existing->origin!=AdmissionOrigin::local_created||!existing->allowed_history.empty()||existing->record_generation!=0){
        status_.admission_stored=false;status_.profile_sync_blocked=true;status_.durability=SlotDurability::not_published;
    }
    auto next=insert_local_creation_record(selected_.snapshot,*creation_,limits_,2);if(!next.ok)return block(next.error.original.code);
    if(next.value.generation==selected_.snapshot.generation){status_.admission_stored=true;status_.profile_sync_blocked=false;status_.phase=ProfileAdmissionPhase::stored;status_.profile_generation=next.value.generation;status_.code.clear();return {true,{},{}};}
    return begin_metadata_candidate(std::move(next.value),*creation_identity_,creation_->capsule.admission_id);
}
Status ResidentProfileCoordinator::begin_metadata_candidate(ProfileMetadataSnapshot&& candidate,const WorldIdentity& source,const StableId& admission_id){
    if(closing_||operation_||!loaded_||!banks_)return profile_admission_failure("ProfileOperationNotReady");
    if(selected_.snapshot.generation==INT64_MAX)return profile_admission_failure("ProfileGenerationOverflow");
    if(candidate.generation!=selected_.snapshot.generation+1||!same_profile_namespace(candidate.name_space,host_namespace_)||!find_profile_admission(candidate,admission_id))return profile_admission_failure("ProfilePendingReceiptMismatch");
    if(selected_.snapshot.generation==INT64_MAX)return profile_admission_failure("ProfileGenerationOverflow");
    std::shared_ptr<CheckpointMemoryResource> resource;if(metadata_job_){auto memory=checkpoint_memory_resource(*metadata_job_,CheckpointJobUse::candidate);if(!memory.ok){status_.fixed_failure=memory.error.code();status_.job_failure=std::move(memory.error);return block("CheckpointJobFailed");}resource=std::move(memory.value);metadata_resource_=resource;}ScopedCheckpointMemory scope(std::move(resource));
    if(metadata_job_&&candidate.wire_version==2){auto encoded=encode_profile_metadata_complete(candidate,limits_,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*metadata_job_);if(!encoded.ok){status_.fixed_failure=encoded.error.code();status_.job_failure=std::move(encoded.error);return block("CheckpointJobFailed");}frozen_job_bytes_=std::move(encoded.value);Bytes().swap(frozen_bytes_);}
    else{auto encoded=encode_profile_metadata(candidate,limits_);if(!encoded.ok)return block(encoded.error.original.code);frozen_bytes_=std::move(encoded.value);metadata_job_.reset();frozen_job_bytes_={};}
    const auto& bytes=frozen_profile_bytes();
    struct PreparedBanks{CheckpointMemoryCharge memory;SlotBankPair value;};auto prepared_banks=checkpoint_make_shared<PreparedBanks>();prepared_banks->memory=CheckpointMemoryCharge(0);prepared_banks->value=*banks_;auto& bank=selected_.bank==SlotBank::a?prepared_banks->value.b:prepared_banks->value.a;Bytes fresh(bytes);bank.bytes.swap(fresh);Bytes().swap(fresh);bank.present=true;
    if(pending_delivery_receipt_)pending_delivery_receipt_->verified_digest=profile_sha256(bytes.data(),bytes.size());
    auto id_memory=CheckpointMemoryCharge(0);StableId fresh_id(admission_id);StableId().swap(pending_admission_id_);metadata_transport_memory_=std::move(id_memory);pending_admission_id_=std::move(fresh_id);next_=std::move(candidate);next_banks_=std::shared_ptr<const SlotBankPair>(prepared_banks,&prepared_banks->value);pending_source_=source;
    if(metadata_job_){auto marker=metadata_job_->reserve(CheckpointJobUse::candidate,0);if(!marker.ok){status_.fixed_failure=marker.error.code();status_.job_failure=std::move(marker.error);return block("CheckpointJobFailed");}next_banks_ownership_=checkpoint_make_shared<JobSlotBankPair>(JobSlotBankPair::Key{},next_banks_,std::move(marker.value),checkpoint_shared_allocation_owner(prepared_banks));}else next_banks_ownership_.reset();
    // 此栈上的请求与服务接纳副本是两份真实分配，分别先预留，不共享可写字节别名。
    auto request_memory=CheckpointMemoryCharge(0);
    SlotWriteRequest request;request.slot={"profile_metadata",SlotFileKind::profile_metadata,namespace_binding_};request.inactive=selected_.bank==SlotBank::a?SlotBank::b:SlotBank::a;request.source=source;request.expected_generation=selected_.snapshot.generation;request.new_generation=next_->generation;request.expected=*banks_;request.bytes=bytes;
    auto accepted=metadata_job_?storage_.stage_profile_metadata(request,metadata_job_):storage_.stage(request);if(!accepted.ok){status_.job_failure=accepted.take_failure();status_.fixed_failure=accepted.fixed_failure;return block(accepted.code.empty()?"CheckpointJobFailed":accepted.code);}operation_=accepted.reference;reading_=false;status_.source=source;status_.phase=ProfileAdmissionPhase::staging;status_.durability=SlotDurability::not_published;status_.system_error=0;status_.code.clear();return {true,{},{}};
}
Status ResidentProfileCoordinator::begin_merge(const CommittedProfileDeliveries& batch,Integer expected){try{
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!active_capsule()||!active_identity_||!loaded_||closing_||operation_)return profile_admission_failure("ProfileMergeNotReady");
    auto current=session_.identity();if(!current.ok)return {false,{},current.error};const auto& source=batch.source();if(source.world_id!=active_identity_->world_id||source.execution_epoch!=active_identity_->execution_epoch||source.world_id!=current.value.world_id||source.execution_epoch!=current.value.execution_epoch||source.revision>current.value.revision||source.tick>current.value.tick)return profile_admission_failure("ProfileSnapshotChanged");
    // 可捕获原胶囊不授予同步权；受阻时不得建立 META 写候选。
    if(status_.profile_sync_blocked)return profile_admission_failure("ProfileSyncBlocked");
    auto next=merge_profile_deliveries(selected_.snapshot,active_capsule()->admission_id,expected,batch,limits_);if(!next.ok)return block(next.error.original.code);return begin_metadata_candidate(std::move(next.value.snapshot),source,active_capsule()->admission_id);
}catch(const std::bad_alloc&){return block("ProfileAdmissionResourceExhausted");}}

Status ResidentProfileCoordinator::complete_write(const StoragePoll& result){
    if(!next_||!pending_source_||pending_admission_id_.empty()||!result.frozen_write||result.frozen_write->bytes!=frozen_profile_bytes())return block("ProfileStoredIdentityMismatch");
    if(result.durability!=SlotDurability::durable_confirmed)return block("ProfileDurabilityUnconfirmed");
    // 已实际发布的同代字节成为下一次CAS观察，原另一银行仍逐字节保留。
    if(!next_banks_)return block("ProfileStoredIdentityMismatch");
    banks_=std::move(next_banks_);banks_ownership_=std::move(next_banks_ownership_);selected_.ok=true;selected_.not_found=false;selected_.bank=result.frozen_write->inactive;selected_.snapshot=std::move(*next_);next_.reset();Bytes().swap(frozen_bytes_);frozen_job_bytes_={};status_.admission_stored=true;status_.profile_sync_blocked=false;status_.phase=ProfileAdmissionPhase::stored;status_.durability=result.durability;status_.profile_generation=selected_.snapshot.generation;status_.code.clear();delivered_receipt_=std::move(pending_delivery_receipt_);return {true,{},{}};
}
Status ResidentProfileCoordinator::advance(){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");
    if(pending_import_)return advance_world_import();
    if(pending_creation_)return advance_creation_proof();
    return advance_metadata();
}
Status ResidentProfileCoordinator::advance_metadata(){try{
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");
    if(!operation_)return {true,{},{}};
    // 所有候选/核验临时均沿本次已拥有resource，worker取消/恢复不依赖另建job。
    ScopedCheckpointMemory scope(metadata_resource_);
    auto result=storage_.poll(*operation_);
    if(!result.ok)return block(result.code);
    status_.durability=result.durability;status_.system_error=result.system_error;
    if(result.state==StorageState::queued||result.state==StorageState::running)return {true,{},{}};
    if(reading_){if(result.state!=StorageState::completed){if(result.state==StorageState::failed||result.state==StorageState::cancelled||result.state==StorageState::closed_unconfirmed){const auto code=result.code.empty()?"ProfileReadFailed":result.code;storage_.release(*operation_);operation_.reset();release_profile_transaction_inputs();return block(code);}return {true,{},{}};}
        if(!result.banks){storage_.release(*operation_);operation_.reset();return block("ProfileReadMissingBanks");}ProfileMetadataSelected selected;if(metadata_job_){if(!result.job_banks){auto released=storage_.release(*operation_);if(released.ok)operation_.reset();return block("ProfileJobReadbackRequired");}auto value=select_profile_metadata(*result.job_banks,host_namespace_,limits_,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*metadata_job_);if(!value.ok){status_.fixed_failure=value.error.code();status_.job_failure=std::move(value.error);auto released=storage_.release(*operation_);if(released.ok)operation_.reset();return block("ProfileMetadataValidationFailed");}selected=std::move(value.value);}else selected=select_profile_metadata(*result.banks,host_namespace_,limits_);auto released=storage_.release(*operation_);if(!released.ok)return block(released.code);operation_.reset();if(!selected.ok)return block(selected.code);banks_=result.banks;banks_ownership_=result.job_banks;selected_=std::move(selected);loaded_=true;reading_=false;status_.profile_generation=selected_.snapshot.generation;status_.phase=ProfileAdmissionPhase::idle;status_.code.clear();if(pending_import_)return continue_import_admission_write();if(creation_)return begin_pending_write();return {true,{},{}};
    }
    if(result.state==StorageState::awaiting_owner_validation){
        status_.phase=ProfileAdmissionPhase::validating;
        // 形状核不足以发布；必须整包等于由原创建回执及已读前代生成的同一冻结候选。
        if(!pending_source_||pending_admission_id_.empty()||!next_||!result.frozen_write||result.frozen_write->bytes!=frozen_profile_bytes()||!identity_equal(result.frozen_write->source,*pending_source_)||result.frozen_write->expected_generation!=selected_.snapshot.generation||result.frozen_write->new_generation!=next_->generation||result.frozen_write->slot.profile_namespace_binding!=namespace_binding_){storage_.cancel(*operation_);return block("ProfilePendingReceiptMismatch");}
        ProfileMetadataDecoded decoded;if(metadata_job_&&next_->wire_version==2){auto value=decode_profile_metadata_complete(frozen_job_bytes_,limits_,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*metadata_job_);if(!value.ok){status_.fixed_failure=value.error.code();status_.job_failure=std::move(value.error);storage_.cancel(*operation_);return block("CheckpointJobFailed");}decoded.ok=true;decoded.snapshot=std::move(value.value);}else decoded=decode_profile_metadata(result.frozen_write->bytes,limits_);const auto* record=decoded.ok?find_profile_admission(decoded.snapshot,pending_admission_id_):nullptr;if(!decoded.ok||!record||!same_admission_capsule(record->capsule,find_profile_admission(*next_,pending_admission_id_)->capsule)||!same_profile_namespace(decoded.snapshot.name_space,host_namespace_)||decoded.snapshot.generation!=next_->generation){storage_.cancel(*operation_);return block(decoded.ok?"ProfilePendingReceiptMismatch":decoded.code);}
        auto proof_memory=CheckpointMemoryCharge(0);
        ProfileValidatedStage proof(result.frozen_write,operation_->instance,operation_->operation,result.stage_token,namespace_binding_,selected_.snapshot.generation,next_->generation,pending_admission_id_);auto promoted=storage_.promote_profile_metadata(*operation_,std::move(proof));if(!promoted.ok){storage_.cancel(*operation_);return block(promoted.code);}status_.phase=ProfileAdmissionPhase::publishing;return {true,{},{}};
    }
    if(result.state==StorageState::awaiting_recovery){status_.phase=ProfileAdmissionPhase::awaiting_recovery;status_.code=result.code;status_.profile_sync_blocked=true;return {true,{},{}};}
    if(result.state==StorageState::completed){auto completed=complete_write(result);auto released=storage_.release(*operation_);if(!released.ok)return block(released.code);operation_.reset();release_profile_transaction_inputs();return completed;}
    if(result.state==StorageState::cancelled||result.state==StorageState::failed||result.state==StorageState::closed_unconfirmed){const auto code=result.code.empty()?"ProfileWriteFailed":result.code;storage_.release(*operation_);operation_.reset();release_profile_transaction_inputs();return block(code);}return {true,{},{}};
}catch(const std::bad_alloc&){return block("ProfileAdmissionResourceExhausted");}}
Status ResidentProfileCoordinator::retry_admission(){
    if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");
    if(closing_)return profile_admission_failure("ProfileClosing");
    if(operation_){auto poll=storage_.poll(*operation_);
    if(poll.ok&&poll.state==StorageState::awaiting_recovery){auto recovered=storage_.recover_same_bytes(*operation_,poll.stage_token);
    if(!recovered.ok)return block(recovered.code);
    status_.phase=ProfileAdmissionPhase::publishing;
    return {true,{},{}};
    }return profile_admission_failure("ProfileOperationPending");
    }
    if(!creation_proven_)return start_creation_proof();return begin_read();
}
Status ResidentProfileCoordinator::cancel(){if(!owner_thread())return profile_admission_failure("ProfileOwnerThreadRequired");if(!operation_)return profile_admission_failure("ProfileOperationNotPending");auto cancelled=storage_.cancel(*operation_);return cancelled.ok?Status{true,{},{}}:profile_admission_failure(cancelled.code.c_str());}
void ResidentProfileCoordinator::request_close(){closing_=true;storage_.request_close();}
bool ResidentProfileCoordinator::close_complete()const{return storage_.close_complete();}
Outcome<AdmissionRecord> ResidentProfileCoordinator::read_admission(const StableId& id)const{if(!owner_thread())return {false,{},profile_admission_failure("ProfileOwnerThreadRequired").error};if(!loaded_)return {false,{},profile_admission_failure("ProfileStoreNotLoaded").error};const auto* r=find_profile_admission(selected_.snapshot,id);if(!r)return {false,{},profile_admission_failure("AdmissionRecoveryRequired").error};return {true,*r,{}};}
Outcome<CapturedProfileAdmission> ResidentProfileCoordinator::capture(const WorldIdentity& source)const{try{
    if(!owner_thread())return {false,{},profile_admission_failure("ProfileOwnerThreadRequired").error};
    if(!active_capsule()||!active_identity_)return {false,{},profile_admission_failure("AdmissionRecoveryRequired").error};
    auto identity=session_.identity();
    if(!identity.ok)return {false,{},identity.error};
    if(!identity_equal(identity.value,source)||source.world_id!=active_identity_->world_id||source.execution_epoch!=active_identity_->execution_epoch)return {false,{},profile_admission_failure("ProfileSnapshotChanged").error};
    const auto* record=find_profile_admission(selected_.snapshot,active_capsule()->admission_id);const bool blocked=status_.profile_sync_blocked||!record||!same_admission_capsule(record->capsule,*active_capsule());
    return {true,{source,*active_capsule(),blocked},{}};
}catch(const std::bad_alloc&){return {false,{},profile_admission_failure("ProfileAdmissionResourceExhausted").error};}}
// 不先复制active capsule；同线程核原身份后在同job直接生成冻结字节。
CheckpointJobResult<CapturedProfileAdmissionBytes> ResidentProfileCoordinator::capture_bytes(const WorldIdentity& source,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job)const{
    try{if(!owner_thread())return {false,{},job.capture_failure(profile_admission_failure("ProfileOwnerThreadRequired").error)};
        if(closing_)return {false,{},job.capture_failure(profile_admission_failure("ProfileClosing").error)};
        if(!active_capsule()||!active_identity_)return {false,{},job.capture_failure(profile_admission_failure("AdmissionRecoveryRequired").error)};
        auto identity=session_.identity();if(!identity.ok)return {false,{},job.capture_failure(std::move(identity.error))};
        if(!identity_equal(identity.value,source)||source.world_id!=active_identity_->world_id||source.execution_epoch!=active_identity_->execution_epoch)return {false,{},job.capture_failure(profile_admission_failure("ProfileSnapshotChanged").error)};
        auto bytes=active_job_capsule_?encode_slot_admission_capsule(*active_job_capsule_,capacity,slot,job):encode_slot_admission_capsule(*active_capsule(),capacity,slot,job);if(!bytes.ok)return {false,{},std::move(bytes.error)};const auto* record=find_profile_admission(selected_.snapshot,active_capsule()->admission_id);const bool blocked=status_.profile_sync_blocked||!record||!same_admission_capsule(record->capsule,*active_capsule());CapturedProfileAdmissionBytes result{source,std::move(bytes.value),blocked};return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
// 仅本机槽继承关联：缺record与导入介质失败是不同入口，不能以此跳过用户确认。
CheckpointJobResult<ProfileWorldInstallResult> ResidentProfileCoordinator::install_local_candidate(WorldRestoreCandidate&& candidate,JobAdmissionCapsule&& capsule,const std::optional<WorldIdentity>& expected,WorldInstallCancellation& cancellation,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{
        if(!owner_thread())return {false,{},job.capture_failure(profile_admission_failure("ProfileOwnerThreadRequired").error)};
        if(closing_||operation_||creation_)return {false,{},job.capture_failure(profile_admission_failure("ProfileRestoreNotReady").error)};
        if(!loaded_)return {false,{},job.capture_failure(profile_admission_failure("ProfileStoreNotLoaded").error)};
        auto checked=job.check_profile(capacity);if(!checked.ok)return {false,{},std::move(checked.error)};checked=job.check_profile(slot);if(!checked.ok)return {false,{},std::move(checked.error)};
        if(!candidate.validated()||!capsule.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        const auto& value=capsule.value();auto original=candidate.verify_admission_baseline(value.source_world_id,value.baseline,capacity,job);if(!original.ok)return {false,{},std::move(original.error)};
        const auto* record=find_profile_admission(selected_.snapshot,value.admission_id);auto association=verify_local_admission(value,candidate.source_identity().world_id,host_namespace_,record,limits_.admission);
        const bool missing=!association.ok&&association.error.original.code=="AdmissionRecoveryRequired";
        if(!association.ok&&!missing)return {false,{},job.capture_failure(std::move(association.error))};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        // 状态拥有值在唯一安装前建完；普通Error/字符串不在世界发布之后构造。
        ProfileAdmissionStatus next;next.world_committed=true;next.admission_stored=!missing;next.profile_sync_blocked=missing;next.phase=missing?ProfileAdmissionPhase::blocked:ProfileAdmissionPhase::stored;next.profile_generation=selected_.snapshot.generation;next.admission_id=value.admission_id;if(missing)next.code="AdmissionRecoveryRequired";
        auto installed=session_.install_storage_candidate(std::move(candidate),expected,cancellation,job);if(!installed.ok)return {false,{},std::move(installed.error)};
        // 从这里起全为noexcept移动/标量；胶囊自身reservation随活动关联继续存活。
        next.source=installed.value;active_capsule_.reset();active_job_capsule_.reset();active_job_capsule_.emplace(std::move(capsule));active_identity_=installed.value;
        static_assert(std::is_nothrow_move_constructible<ProfileAdmissionStatus>::value,"post-install status transfer");status_.~ProfileAdmissionStatus();new(&status_)ProfileAdmissionStatus(std::move(next));
        ProfileWorldInstallResult result{installed.value,selected_.snapshot.generation,!missing,missing};return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
ProfileAdmissionStatus ResidentProfileCoordinator::restore_local_world(const BoundaryValue& catalog,const Bytes& checkpoint,const Bytes& continuation,const Bytes& capsule_bytes){try{
    if(!owner_thread()||closing_||operation_||creation_||active_capsule()){auto out=status_;out.code="ProfileRestoreNotReady";return out;}if(!loaded_){auto out=status_;out.code="ProfileStoreNotLoaded";return out;}
    auto capsule=decode_slot_admission_capsule(capsule_bytes,limits_.admission);if(!capsule.ok){status_.code=capsule.error.original.code;return status_;}if(!capsule.value.baseline.empty()){status_.code="UnsupportedProfileDeliveryProjection";return status_;}
    auto checked=inspect(checkpoint);if(!checked.ok){status_.code=checked.error.original.code;return status_;}const auto* record=find_profile_admission(selected_.snapshot,capsule.value.admission_id);auto association=verify_local_admission(capsule.value,checked.value.identity.world_id,host_namespace_,record,limits_.admission);const bool missing=!association.ok&&association.error.original.code=="AdmissionRecoveryRequired";if(!association.ok&&!missing){status_.code=association.error.original.code;return status_;}
    // 缺record不造同意；先预备完整关联值，核心冷校验仍只发布一次、不推进时间。
    SlotAdmissionCapsule frozen=std::move(capsule.value);auto restored=session_.initialize_checkpoint_with_continuation(catalog,checkpoint,continuation);if(!restored.ok){status_.code=restored.error.original.code;return status_;}status_.world_committed=true;auto identity=session_.identity();if(!identity.ok){block(identity.error.original.code);return status_;}active_capsule_=std::move(frozen);active_identity_=identity.value;status_.source=identity.value;status_.admission_id=active_capsule()->admission_id;status_.admission_stored=!missing;status_.profile_sync_blocked=true;
    // record只证明本机关联；已安装世界仍须实际证明本片没有delivery producer，才可解除同步阻断。
    auto proof=session_.inspect_profile_delivery_absence();
    if(!proof.ok){block(proof.error.original.code);return status_;}
    auto after=session_.identity();
    if(!after.ok||!identity_equal(after.value,identity.value)||!identity_equal(proof.value.source_identity,identity.value)||proof.value.profile_id!=exact_rest_profile_id||!catalog_equal(proof.value.catalog,checked.value.identity.catalog)){
        block("ProfileSnapshotChanged");return status_;
    }
    status_.profile_sync_blocked=missing;status_.phase=missing?ProfileAdmissionPhase::blocked:ProfileAdmissionPhase::stored;status_.code=missing?"AdmissionRecoveryRequired":"";return status_;
}catch(checkpoint_job_detail::FailureException& failure){
    status_.fixed_failure=failure.error.code();status_.job_failure=std::move(failure.error);
    status_.phase=ProfileAdmissionPhase::blocked;status_.profile_sync_blocked=true;return status_;
}catch(const std::bad_alloc&){block("ProfileAdmissionResourceExhausted");return status_;}}
}
