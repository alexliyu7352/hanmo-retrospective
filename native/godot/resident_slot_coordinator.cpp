// 同一真实core的捕获、读回核验与内部介质提升闭环；不接纳外部自报payload或validated标志。
#include "resident_slot_coordinator.hpp"
#include "resident_profile_coordinator.hpp"
#include "resident_slot_envelope.hpp"
#include "resident/kernel/world_restore_retention.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"

namespace hanmo::resident::storage {
namespace {
// 完整运输来源与操作号固定绑定，不能用新epoch的新票据补旧保存。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// coordinator逻辑票据不随底层inspect/stage操作号切换而重绑。
bool same_reference(const StorageOperationRef& a,const StorageOperationRef& b){return a.instance==b.instance&&a.operation==b.operation&&same_identity(a.source,b.source);}
// unknown关闭后可领取；没有把它归入completed或cancelled。
bool done(SlotSaveState state){return state==SlotSaveState::completed||state==SlotSaveState::cancelled||state==SlotSaveState::failed||state==SlotSaveState::closed_unconfirmed;}
}
// 捕获来自真实execute_live且整对冻结；原观察字节只供精确CAS，绝不成为第二世界。
struct ResidentSlotCoordinator::Pending {
    StorageOperationRef reference,transport;SlotKey slot;Integer expected_generation=0;
    std::shared_ptr<const LiveOperationResult> capture;
    std::shared_ptr<const SlotBankPair> banks;
    SlotSaveState phase=SlotSaveState::inspecting;bool publication_requested=false;
    StorageDiagnostic failure_code;StorageCoreError failure;
};
// 原worker结果只封闭拥有私有candidate；poll不暴露世界、fd或可提升token。
class SlotLoadAnalysisResult final:public StorageStreamAnalysisResult {
    friend class ResidentSlotCoordinator;friend class SlotLoadAnalysis;
    mutable std::shared_ptr<const WorldRestoreRetention> retention_;
    mutable WorldRestoreCandidate candidate_;
    mutable JobAdmissionCapsule capsule_;
    Integer generation_=0;mutable bool consumed_=false;
};
class SlotLoadAnalysis final:public StorageStreamAnalysis {
public:
    explicit SlotLoadAnalysis(std::shared_ptr<const WorldRestoreRetention> retention,std::shared_ptr<const PreparedCatalog> catalog):retention_(std::move(retention)),catalog_(std::move(catalog)){}
    CheckpointJobResult<std::shared_ptr<const StorageStreamAnalysisResult>> analyze(const SlotKey& slot,const SlotStreamBankPair* banks,SlotReadLease* staged,CheckpointJobBudget& job,const SlotCancellation& cancellation)const override{
        try{if(staged||!banks)return {false,{},job.capture_boundary_failure("StorageInvalidLoadSource",{},ErrorKind::rule)};
            if(cancellation.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            const auto& capacity=engineering_checkpoint_capacity_profile();const auto& limits=engineering_slot_capacity_profile();
            auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
            auto chosen=select_slot_envelope_banks(*banks,slot.id,capacity,limits,job);if(!chosen.ok)return {false,{},std::move(chosen.error)};if(!chosen.value.present)return {false,{},job.capture_boundary_failure("SlotNotFound",{},ErrorKind::rule)};
            const auto& index=chosen.value.inspection.value();auto source=chosen.value.bank==SlotBank::a?banks->a:banks->b;if(!source)return {false,{},job.capture_boundary_failure("StorageInvalidLoadSource",{},ErrorKind::rule)};
            SlotPayloadReadSource checkpoint(*source,index.payloads[0]);const auto& c=index.payloads[1];BoundContinuationSource continuation{c.present?source.get():nullptr,c.offset,c.length,c.sha256};
            auto candidate=WorldRestoreCandidate::prepare(checkpoint,index.payloads[0].sha256,catalog_,continuation,capacity,limits,job);if(!candidate.ok)return {false,{},std::move(candidate.error)};
            const auto& a=index.payloads[2];auto admission=decode_slot_admission_capsule(*source,a.offset,a.length,a.sha256,capacity,limits,job);if(!admission.ok)return {false,{},std::move(admission.error)};
            // 同实际私有world核source和baseline，metadata记录权限仍由所属线程真正profile库核。
            auto valid=candidate.value.verify_admission_baseline(admission.value.value().source_world_id,admission.value.value().baseline,capacity,job);if(!valid.ok)return {false,{},std::move(valid.error)};
            if(cancellation.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            auto result=checkpoint_make_unique<SlotLoadAnalysisResult>();result->retention_=retention_;result->candidate_=std::move(candidate.value);result->capsule_=std::move(admission.value);result->generation_=index.generation;
            std::shared_ptr<const StorageStreamAnalysisResult> owned=checkpoint_share(std::move(result));return {true,std::move(owned),{}};
        }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
    }
private:std::shared_ptr<const WorldRestoreRetention> retention_;std::shared_ptr<const PreparedCatalog> catalog_;
};
struct ResidentSlotCoordinator::PendingLoad {
    StorageOperationRef reference;std::optional<WorldIdentity> expected_active;
    ResidentReplaySession* session=nullptr;std::shared_ptr<CheckpointJobBudget> job;
    std::shared_ptr<const WorldRestoreRetention> retention;
    std::shared_ptr<const SlotLoadAnalysis> analysis;std::shared_ptr<WorldInstallCancellation> cancellation;
    CheckpointJobFailure failure;
    SlotLoadState phase=SlotLoadState::inspecting;
};
// 宿主在world线程建立本消费者；另一个专用线程仅负责真正文件I/O。
ResidentSlotCoordinator::ResidentSlotCoordinator(std::string directory,StorageServiceLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
,SlotIoTestHooks hooks,SlotCoordinatorTestHooks coordinator_hooks
#endif
):storage_(std::move(directory),limits
#ifdef HANMO_STORAGE_TEST_FAULTS
,std::move(hooks)
#endif
),owner_(std::this_thread::get_id())
#ifdef HANMO_STORAGE_TEST_FAULTS
,test_hooks_(std::move(coordinator_hooks))
#endif
{}
// 正常宿主先close+advance领取终态再销毁；底层析构join仍只是内存安全兜底。
ResidentSlotCoordinator::~ResidentSlotCoordinator()=default;
// 不把session的可变世界引用送给存储线程或其它运输线程。
bool ResidentSlotCoordinator::owner_thread()const noexcept{return std::this_thread::get_id()==owner_;}
// 只共享有限状态缓存，所有大字节及核心步骤仍由world线程独占。
void ResidentSlotCoordinator::publish(SlotSaveState state,StorageDiagnostic code,SlotDurability durability,int system_error,StorageCoreError error){
    pending_->phase=state;std::lock_guard<std::mutex> lock(mutex_);cached_.ok=true;cached_.reference=pending_->reference;cached_.generation=pending_->expected_generation+1;cached_.state=state;cached_.code=std::move(code);cached_.durability=durability;cached_.system_error=system_error;cached_.core_error=std::move(error);
}
// 唯一保存入口在真实core捕获source和完整payload/continuation，不允许外部替换或省略其一。
StorageAccepted ResidentSlotCoordinator::begin_save(ResidentReplaySession& session,const std::string& slot,Integer expected_generation){
    if(!owner_thread())return {false,{},"StorageWrongOwnerThread"};
    if(closing_)return {false,{},"StorageClosing"};
    if(pending_||pending_load_||pending_stream_)return {false,{},"StorageBusy"};
    if(!ResidentSlotMedia::valid_slot(slot))return {false,{},"InvalidSlot"};
    if(expected_generation<0||expected_generation>=2147483647)return {false,{},"InvalidGeneration"};
    try{
        auto identity=session.identity();if(!identity.ok)return {false,{},identity.error.original.code};
        LiveOperation operation;operation.kind=LiveOperationKind::checkpoint;operation.source=identity.value;operation.payload=BoundaryValue{BoundaryValue::Dict{}};
        auto captured=session.execute_live(operation);if(!captured.ok)return {false,{},captured.error.original.code};
        if(!captured.value.checkpoint_bytes||captured.value.checkpoint_bytes->empty()||!same_identity(captured.value.source_identity,identity.value)||!same_identity(captured.value.current_identity,identity.value))return {false,{},"StorageInvalidCoreCapture"};
        auto record=std::make_unique<Pending>();record->slot={slot,SlotFileKind::resident_checkpoint};record->expected_generation=expected_generation;record->capture=std::make_shared<const LiveOperationResult>(std::move(captured.value));
        auto read=storage_.inspect(record->slot,identity.value);if(!read.ok)return read;
        record->reference=record->transport=read.reference;pending_=std::move(record);cancelling_=false;
        {std::lock_guard<std::mutex> lock(mutex_);active_=read.reference;}
        #ifdef HANMO_STORAGE_TEST_FAULTS
        if(test_hooks_.after_inspect_accepted)test_hooks_.after_inspect_accepted();
#endif
        // 接纳后的提交只移动已备对象、赋值有限身份和空状态，不再分配字节或字符串。
        publish(SlotSaveState::inspecting);return {true,read.reference,{}};
    }catch(const std::bad_alloc&){
        // 一旦底层已接纳，必须返回原票据让调用方领取取消/清理结果，不能制造无主请求。
        if(pending_){reject_staged("StorageResourceLimit");return {true,pending_->reference,{}};}
        return {false,{},"StorageResourceLimit"};
    }
}
// stage尚未接纳时没有tmp或发布；原读取记录可安全释放，真实核心错误保留原码。
void ResidentSlotCoordinator::fail_before_stage(StorageDiagnostic code,StorageCoreError error){
    auto released=storage_.release(pending_->transport);
    if(!released.ok&&released.code=="StorageOperationPending"){reject_staged(std::move(code),std::move(error));return;}
    pending_->banks.reset();pending_->capture.reset();
    {std::lock_guard<std::mutex> lock(mutex_);active_.reset();}
    const auto state=code=="StorageCancelled"?SlotSaveState::cancelled:SlotSaveState::failed;
    publish(state,std::move(code),SlotDurability::not_published,0,std::move(error));
}
// 原核验失败后仍让存储worker准确清理；不能在未清理前发布取消成功。
void ResidentSlotCoordinator::reject_staged(StorageDiagnostic code,StorageCoreError error){
    pending_->failure_code=std::move(code);pending_->failure=std::move(error);storage_.cancel(pending_->transport);publish(SlotSaveState::cancelling);
}
// 相同实际bytes的四摘要与完整source必须都匹配真实core返回；任何字段不能从外部填bool替代。
bool ResidentSlotCoordinator::validated(ResidentReplaySession& session,const Bytes& candidate,const std::optional<Bytes>& continuation,const ResidentSlotSelected& previous){
    std::optional<Bytes> old,old_continuation;if(previous.ok){old=previous.envelope.payload;old_continuation=previous.envelope.continuation;}
    auto checked=session.validate_storage_checkpoint(pending_->reference.source,candidate,continuation,old,old_continuation);
    if(!checked.ok){pending_->failure_code=checked.error.original.code;pending_->failure=checked.error;return false;}
    const auto& proof=checked.value;const auto& source=pending_->reference.source;
    if(!same_identity(proof.source_identity,source)||proof.current_identity.world_id!=source.world_id||proof.current_identity.execution_epoch!=source.execution_epoch||proof.candidate_sha256!=resident_slot_sha256(candidate)||proof.candidate_continuation_sha256!=(continuation?resident_slot_sha256(*continuation):std::string{})||proof.previous_sha256!=(old?resident_slot_sha256(*old):std::string{})||proof.previous_continuation_sha256!=(old_continuation?resident_slot_sha256(*old_continuation):std::string{})){
        pending_->failure_code="NativeValidationMismatch";pending_->failure.reset();return false;
    }
    return true;
}
// 底层终态原样保可见/耐久；原核验失败与介质清理失败均不能伪装保存完成。
void ResidentSlotCoordinator::finish(const StoragePoll& result){
    SlotSaveState state=SlotSaveState::failed;StorageDiagnostic code=result.code;StorageCoreError error;
    if(result.state==StorageState::completed){
        if(pending_->publication_requested)state=SlotSaveState::completed;
        else{state=pending_->failure_code=="StorageCancelled"?SlotSaveState::cancelled:SlotSaveState::failed;code=pending_->failure_code.empty()?"StorageInvalidStage":pending_->failure_code;error=pending_->failure;}
    }
    else if(result.state==StorageState::closed_unconfirmed)state=SlotSaveState::closed_unconfirmed;
    else if(result.state==StorageState::awaiting_recovery){publish(SlotSaveState::awaiting_recovery,std::move(code),result.durability,result.system_error);return;}
    else if(result.state==StorageState::cancelled){state=SlotSaveState::cancelled;if(!pending_->failure_code.empty()&&pending_->failure_code!="StorageCancelled"){state=SlotSaveState::failed;code=pending_->failure_code;error=pending_->failure;}}
    storage_.release(pending_->transport);pending_->banks.reset();pending_->capture.reset();
    {std::lock_guard<std::mutex> lock(mutex_);active_.reset();}
    publish(state,std::move(code),result.durability,result.system_error,std::move(error));
}
// 每次只推进一个阶段，暂停点使同epoch推进、换epoch和取消都能在真实边界复核。
StorageStatus ResidentSlotCoordinator::advance(ResidentReplaySession& session){
    if(pending_stream_)return advance_stream_save(session);
    if(!owner_thread())return {false,"StorageWrongOwnerThread"};
    if(!pending_)return {false,"StorageNoOperation"};
    if(done(pending_->phase))return {true,{}};
    try{
        #ifdef HANMO_STORAGE_TEST_FAULTS
        if(test_hooks_.before_poll)test_hooks_.before_poll();
#endif
        const auto phase=pending_->phase;auto result=storage_.poll(pending_->transport);if(!result.ok)return {false,result.code};
        if(phase==SlotSaveState::inspecting){
            if(result.state==StorageState::queued||result.state==StorageState::running)return {true,{}};
            if(result.state!=StorageState::completed){finish(result);return {true,{}};}
            if(cancelling_||closing_){fail_before_stage("StorageCancelled");return {true,{}};}
            pending_->banks=result.banks;publish(SlotSaveState::awaiting_preflight);return {true,{}};
        }
        if(phase==SlotSaveState::awaiting_preflight){
            if(cancelling_||closing_){fail_before_stage("StorageCancelled");return {true,{}};}
            if(!pending_->banks){fail_before_stage("StorageInvalidObservation");return {true,{}};}
            auto previous=select_resident_slot(*pending_->banks,pending_->slot.id);
            if(!previous.ok&&!previous.not_found){fail_before_stage(previous.code);return {true,{}};}
            if((previous.ok?previous.envelope.generation:0)!=pending_->expected_generation){fail_before_stage("GenerationConflict");return {true,{}};}
            const auto& captured=*pending_->capture;
            if(!validated(session,*captured.checkpoint_bytes,captured.continuation_bytes,previous)){fail_before_stage(pending_->failure_code,pending_->failure);return {true,{}};}
            auto envelope=encode_resident_slot(*captured.checkpoint_bytes,pending_->expected_generation+1,pending_->slot.id,captured.continuation_bytes);
            if(!envelope.ok){fail_before_stage(envelope.error.original.code,envelope.error);return {true,{}};}
            if(cancelling_||closing_){fail_before_stage("StorageCancelled");return {true,{}};}
            SlotWriteRequest write;write.slot=pending_->slot;write.source=pending_->reference.source;write.expected_generation=pending_->expected_generation;write.new_generation=pending_->expected_generation+1;write.inactive=previous.ok&&previous.bank==SlotBank::b?SlotBank::a:SlotBank::b;write.expected=*pending_->banks;write.bytes=std::move(envelope.value);
            storage_.release(pending_->transport);auto staged=storage_.stage(write);
            if(!staged.ok){fail_before_stage(staged.code);return {true,{}};}
            pending_->transport=staged.reference;pending_->banks.reset();{std::lock_guard<std::mutex> lock(mutex_);active_=staged.reference;}
            publish(SlotSaveState::staging);if(cancelling_||closing_)storage_.cancel(staged.reference);return {true,{}};
        }
        if(phase==SlotSaveState::staging){
            if(result.state==StorageState::awaiting_owner_validation){publish(SlotSaveState::awaiting_validation);return {true,{}};}
            if(result.state!=StorageState::queued&&result.state!=StorageState::running)finish(result);
            return {true,{}};
        }
        if(phase==SlotSaveState::awaiting_validation){
            if(result.state!=StorageState::awaiting_owner_validation){if(result.state!=StorageState::queued&&result.state!=StorageState::running)finish(result);return {true,{}};}
            if(cancelling_||closing_){reject_staged("StorageCancelled");return {true,{}};}
            if(!result.frozen_write||!same_identity(result.frozen_write->source,pending_->reference.source)){reject_staged("NativeValidationMismatch");return {true,{}};}
            const auto& write=*result.frozen_write;auto candidate=decode_resident_slot(write.bytes);auto previous=select_resident_slot(write.expected,pending_->slot.id);
            if(!candidate.ok){reject_staged(candidate.code);return {true,{}};}
            if(!previous.ok&&!previous.not_found){reject_staged(previous.code);return {true,{}};}
            if(candidate.envelope.save_id!=pending_->slot.id||candidate.envelope.generation!=pending_->expected_generation+1||(previous.ok?previous.envelope.generation:0)!=pending_->expected_generation||candidate.envelope.payload!=*pending_->capture->checkpoint_bytes||candidate.envelope.continuation!=pending_->capture->continuation_bytes){reject_staged("NativeValidationMismatch");return {true,{}};}
            if(!validated(session,candidate.envelope.payload,candidate.envelope.continuation,previous)){reject_staged(pending_->failure_code,pending_->failure);return {true,{}};}
            if(cancelling_||closing_){reject_staged("StorageCancelled");return {true,{}};}
            auto promoted=storage_.promote_after_owner_validation(pending_->transport,result.stage_token);if(!promoted.ok){reject_staged(promoted.code);return {true,{}};}
            pending_->publication_requested=true;publish(SlotSaveState::promoting);return {true,{}};
        }
        if(result.state!=StorageState::queued&&result.state!=StorageState::running&&result.state!=StorageState::awaiting_owner_validation)finish(result);
        return {true,{}};
    }catch(const std::bad_alloc&){if(pending_->phase==SlotSaveState::inspecting||pending_->phase==SlotSaveState::awaiting_preflight)fail_before_stage("StorageResourceLimit");else reject_staged("StorageResourceLimit");return {false,"StorageResourceLimit"};}
}
// 不读取世界或文件，只复制原逻辑票据的有限缓存。
SlotSavePoll ResidentSlotCoordinator::poll(const StorageOperationRef& reference)const{
    std::lock_guard<std::mutex> lock(mutex_);if(!cached_.ok||!same_reference(cached_.reference,reference)){SlotSavePoll denied;denied.code="StorageStaleReference";return denied;}return cached_;
}
// 运输取消可以并发请求；阶段切换后的world线程还会再核取消位，避免新stage漏取消。
StorageStatus ResidentSlotCoordinator::cancel(const StorageOperationRef& reference){
    std::optional<StorageOperationRef> active;SlotSaveState phase;
    {std::lock_guard<std::mutex> lock(mutex_);if(!cached_.ok||!same_reference(cached_.reference,reference))return {false,"StorageStaleReference"};if(done(cached_.state))return {false,"StorageAlreadyCompleted"};phase=cached_.state;active=active_;cancelling_=true;}
    if(active){auto result=storage_.cancel(*active);if(!result.ok&&(result.code=="StoragePublicationStarted"||(result.code=="StorageAlreadyCompleted"&&phase==SlotSaveState::promoting)))return result;}
    return {true,{}};
}
// 立即关闭准入并要求worker清理，world线程随后advance将真实终态送入有限缓存。
void ResidentSlotCoordinator::request_close(){closing_=true;cancelling_=true;{std::lock_guard<std::mutex> lock(mutex_);if(load_cancel_)load_cancel_->request();}storage_.request_close();}
// 两个owner都已准确终结才ack；unknown或清理失败仍保存在回执，不被改叫成功。
bool ResidentSlotCoordinator::close_complete()const{std::lock_guard<std::mutex> lock(mutex_);return storage_.close_complete()&&(!cached_.ok||done(cached_.state))&&(!load_cached_.ok||load_cached_.state==SlotLoadState::installed||load_cached_.state==SlotLoadState::cancelled||load_cached_.state==SlotLoadState::failed);}
// 只对原已核验且已提升的same-bytes补屏障，epoch变化不使它变成另一世界的保存。
StorageStatus ResidentSlotCoordinator::recover_same_bytes(const StorageOperationRef& reference){
    if(pending_stream_)return recover_stream_save(reference);
    if(!owner_thread())return {false,"StorageWrongOwnerThread"};
    if(!pending_||!same_reference(pending_->reference,reference))return {false,"StorageStaleReference"};
    if(pending_->phase!=SlotSaveState::awaiting_recovery)return {false,"StorageInvalidStage"};
    auto result=storage_.poll(pending_->transport);if(!result.ok)return {false,result.code};
    auto recovered=storage_.recover_same_bytes(pending_->transport,result.stage_token);if(recovered.ok)publish(SlotSaveState::promoting,{},SlotDurability::visible_unconfirmed);return recovered;
}
// 大字节只由world线程释放，释放后旧票据明确失效，不覆盖尚未决保存。
StorageStatus ResidentSlotCoordinator::release(const StorageOperationRef& reference){
    if(pending_stream_)return release_stream_save(reference);
    if(!owner_thread())return {false,"StorageWrongOwnerThread"};
    if(!pending_||!same_reference(pending_->reference,reference))return {false,"StorageStaleReference"};
    if(!done(pending_->phase))return {false,"StorageOperationPending"};
    pending_.reset();std::lock_guard<std::mutex> lock(mutex_);cached_={};active_.reset();return {true,{}};
}
// 固定工程profile由native选择；输入文件、GD或MOD不能扩大此job的13/8限额。
StorageAccepted ResidentSlotCoordinator::begin_load(ResidentReplaySession& session,const BoundaryValue& catalog,const StableId& scenario,const CatalogIdentity& expected_catalog,const std::string& slot){
    if(!owner_thread())return {false,{},"StorageWrongOwnerThread"};if(closing_)return {false,{},"StorageClosing"};if(pending_||pending_load_||pending_stream_)return {false,{},"StorageBusy"};if(!ResidentSlotMedia::valid_slot(slot))return {false,{},"InvalidSlot"};
    try{auto made=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());if(!made.ok)return StorageAccepted::rejected(std::move(made.error));
        auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::candidate);if(!resource.ok)return StorageAccepted::rejected(std::move(resource.error));ScopedCheckpointMemory scope(resource.value);
        auto job=checkpoint_share(checkpoint_make_unique<CheckpointJobBudget>(std::move(made.value)));
        auto retained=session.retain_storage_world(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*job);if(!retained.ok)return StorageAccepted::rejected(std::move(retained.error));
        auto pending=checkpoint_share(checkpoint_make_unique<PendingLoad>());pending->job=job;pending->session=&session;pending->retention=std::move(retained.value);pending->expected_active=pending->retention->expected_identity();
        auto prepared=session.prepare_storage_catalog(catalog,scenario,expected_catalog,*job);if(!prepared.ok)return StorageAccepted::rejected(std::move(prepared.error));
        pending->analysis=checkpoint_share(checkpoint_make_unique<SlotLoadAnalysis>(pending->retention,std::move(prepared.value)));pending->cancellation=checkpoint_share(checkpoint_make_unique<WorldInstallCancellation>());
        auto accepted=storage_.inspect_stream({slot,SlotFileKind::slot_envelope_v2},pending->expected_active.value_or(WorldIdentity{}),job);if(!accepted.ok)return accepted;
        // 票据已接纳后的发布只移动已备对象；失败结果不会遗失worker记录。
        pending->reference=accepted.reference;pending_load_=std::move(pending);{std::lock_guard<std::mutex> lock(mutex_);load_cancel_=pending_load_->cancellation;load_cached_={};load_cached_.ok=true;load_cached_.reference=accepted.reference;load_cached_.state=SlotLoadState::inspecting;}return accepted;
    }catch(checkpoint_job_detail::FailureException& error){return StorageAccepted::rejected(std::move(error.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
StorageStatus ResidentSlotCoordinator::advance_load(ResidentReplaySession& session,ResidentProfileCoordinator& profile){
    if(!owner_thread())return {false,"StorageWrongOwnerThread"};if(!pending_load_||pending_load_->session!=&session)return {false,"StorageNoOperation"};if(!profile.owns_session(session))return {false,"StorageSessionMismatch"};auto& pending=*pending_load_;
    if(pending.phase==SlotLoadState::installed||pending.phase==SlotLoadState::failed||pending.phase==SlotLoadState::cancelled)return {true,{}};
    try{auto result=storage_.poll(pending.reference);if(!result.ok)return {false,result.code};if(result.state==StorageState::queued||result.state==StorageState::running)return {true,{}};
    auto terminal=[&](SlotLoadState phase,const StorageDiagnostic& code,CheckpointJobFailureLease failure={}){pending.phase=phase;std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=phase;load_cached_.code=code;load_cached_.failure=std::move(failure);};
    if(pending.cancellation->cancelled()||closing_){terminal(SlotLoadState::cancelled,"StorageCancelled");return {true,{}};}
    if(result.state!=StorageState::completed){terminal(result.state==StorageState::cancelled?SlotLoadState::cancelled:SlotLoadState::failed,result.code,result.job_failure);return {true,{}};}
    if(pending.phase==SlotLoadState::inspecting){auto queued=storage_.analyze_stream(pending.reference,0,pending.analysis);if(!queued.ok){terminal(SlotLoadState::failed,queued.code);return queued;}pending.phase=SlotLoadState::validating;std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=pending.phase;return {true,{}};}
    const auto analysis=std::dynamic_pointer_cast<const SlotLoadAnalysisResult>(result.stream_analysis);if(!analysis||analysis->consumed_){terminal(SlotLoadState::failed,"StorageInvalidAnalysisResult");return {true,{}};}
    if(pending.phase==SlotLoadState::validating){pending.phase=SlotLoadState::awaiting_install;std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=pending.phase;load_cached_.generation=analysis->generation_;return {true,{}};}
    // 本机record关联只在当前host线程读取；candidate/capsule本身来自同一次原worker解析。
    auto installed=profile.install_local_candidate(std::move(analysis->candidate_),std::move(analysis->capsule_),pending.expected_active,*pending.cancellation,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*pending.job);analysis->consumed_=true;
    if(!installed.ok){CheckpointJobFailureLease failure;
        // first failure原租约直接由pending保留，不重构造Error；缓存借原拥有者。
        pending.failure=std::move(installed.error);failure=CheckpointJobFailureLease::retain(pending.failure);terminal(SlotLoadState::failed,"CheckpointJobFailed",std::move(failure));return {true,{}};}
    analysis->retention_.reset();pending.analysis.reset();pending.retention.reset();
    pending.phase=SlotLoadState::installed;{std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=pending.phase;load_cached_.installed_identity=installed.value.source;load_cached_.profile_sync_blocked=installed.value.profile_sync_blocked;load_cached_.code.clear();}return {true,{}};
    }catch(checkpoint_job_detail::FailureException& error){pending.failure=std::move(error.error);pending.phase=SlotLoadState::failed;std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=SlotLoadState::failed;load_cached_.code.clear();load_cached_.failure=CheckpointJobFailureLease::retain(pending.failure);return {false,{}};}
    catch(const std::bad_alloc&){pending.failure=pending.job->capture_fixed_failure(CheckpointJobFailureCode::allocation_failed);pending.phase=SlotLoadState::failed;std::lock_guard<std::mutex> lock(mutex_);load_cached_.state=SlotLoadState::failed;load_cached_.code.clear();load_cached_.failure=CheckpointJobFailureLease::retain(pending.failure);return {false,{}};}
}
SlotLoadPoll ResidentSlotCoordinator::poll_load(const StorageOperationRef& reference)const{std::lock_guard<std::mutex> lock(mutex_);if(!load_cached_.ok||!same_reference(load_cached_.reference,reference)){SlotLoadPoll result;result.code="StorageStaleReference";return result;}return load_cached_;}
StorageStatus ResidentSlotCoordinator::cancel_load(const StorageOperationRef& reference){std::shared_ptr<WorldInstallCancellation> cancellation;{std::lock_guard<std::mutex> lock(mutex_);if(!load_cached_.ok||!same_reference(load_cached_.reference,reference))return {false,"StorageStaleReference"};if(load_cached_.state==SlotLoadState::installed||load_cached_.state==SlotLoadState::failed||load_cached_.state==SlotLoadState::cancelled)return {false,"StorageAlreadyCompleted"};cancellation=load_cancel_;}if(!cancellation||!cancellation->request())return {false,"StoragePublicationStarted"};storage_.cancel(reference);return {true,{}};}
StorageStatus ResidentSlotCoordinator::release_load(const StorageOperationRef& reference){if(!owner_thread())return {false,"StorageWrongOwnerThread"};if(!pending_load_||!same_reference(pending_load_->reference,reference))return {false,"StorageStaleReference"};if(pending_load_->phase!=SlotLoadState::installed&&pending_load_->phase!=SlotLoadState::failed&&pending_load_->phase!=SlotLoadState::cancelled)return {false,"StorageOperationPending"};auto released=storage_.release(reference);if(!released.ok)return released;{std::lock_guard<std::mutex> lock(mutex_);load_cached_={};load_cancel_.reset();}pending_load_->analysis.reset();pending_load_->retention.reset();pending_load_.reset();return {true,{}};}
} // 原核验失败不发布新银行；成功只形成原来源精确字节的介质回执。
