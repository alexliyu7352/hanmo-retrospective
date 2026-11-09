// 同一coordinator/worker的完整三载荷保存；后台只读封闭快照和私有读回候选。
#include "resident_slot_coordinator.hpp"
#include "resident_profile_coordinator.hpp"
#include "resident_stored_slot_receipt.hpp"
#include "resident/kernel/world_checkpoint_capture.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <chrono>
namespace hanmo::resident::storage {
namespace {
bool same(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same(const StorageOperationRef&a,const StorageOperationRef&b){return a.instance==b.instance&&a.operation==b.operation&&same(a.source,b.source);}
bool terminal(SlotSaveState s){return s==SlotSaveState::completed||s==SlotSaveState::failed||s==SlotSaveState::cancelled||s==SlotSaveState::closed_unconfirmed;}
class MeasureSink final:public CheckpointWriteSink {public:Status write(const std::uint8_t*,std::size_t)override{return common_success();}};
class CoreSlotProducer final:public SlotCheckpointProducer {
public:
    explicit CoreSlotProducer(std::shared_ptr<const WorldCheckpointCapture> capture):capture_(std::move(capture)){}
    const CheckpointIdentity& identity()const noexcept override{return capture_->identity();}
    std::uint16_t codec_version()const noexcept override{return 2;}
    CheckpointJobResult<CheckpointStreamReceipt> write(CheckpointWriteSink& out,CheckpointJobBudget& job)const override{return capture_->write(out,engineering_checkpoint_capacity_profile(),job);}
private:std::shared_ptr<const WorldCheckpointCapture> capture_;
};
// 只有本TU真实原codec读回才能创建此结果，coordinator不能从bool造提升证明。
class SaveAnalysisResult final:public StorageStreamAnalysisResult {
public:std::shared_ptr<const FrozenSlotEnvelope> envelope;SlotBank inactive=SlotBank::b;bool staged=false;
};
class SlotSaveAnalysis final:public StorageStreamAnalysis {
public:
    SlotSaveAnalysis(std::shared_ptr<const WorldCheckpointCapture> capture,CheckpointJobBytes&& capsule,Integer expected,std::uint64_t saved)
        :capture_(std::move(capture)),capsule_(std::move(capsule)),expected_(expected),saved_(saved){}
    explicit SlotSaveAnalysis(std::shared_ptr<const WorldCheckpointCapture> capture,std::shared_ptr<const FrozenSlotEnvelope> envelope)
        :capture_(std::move(capture)),frozen_(std::move(envelope)){}
    CheckpointJobResult<std::shared_ptr<const StorageStreamAnalysisResult>> analyze(const SlotKey& key,const SlotStreamBankPair* banks,SlotReadLease* staged,CheckpointJobBudget& job,const SlotCancellation& cancel)const override{
        try{
            if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            const auto& capacity=engineering_checkpoint_capacity_profile();const auto& slot=engineering_slot_capacity_profile();
            auto resource=checkpoint_memory_resource(job,CheckpointJobUse::encode);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
            if(!capture_||!capture_->belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
            auto result=checkpoint_make_shared<SaveAnalysisResult>();
            if(!frozen_){
                if(staged||!banks||capsule_consumed_)return {false,{},job.capture_boundary_failure("StorageInvalidAnalysis",{},ErrorKind::rule)};
                auto selected=select_slot_envelope_banks(*banks,key.id,capacity,slot,job);if(!selected.ok)return {false,{},std::move(selected.error)};
                if((selected.value.present?selected.value.inspection.value().generation:0)!=expected_)return {false,{},job.capture_boundary_failure("GenerationConflict",{},ErrorKind::rule)};
                MeasureSink measure;auto checkpoint=capture_->write(measure,capacity,job);if(!checkpoint.ok)return {false,{},std::move(checkpoint.error)};
                auto continuation=capture_->bind_continuation(checkpoint.value.full_checkpoint_sha256,capacity,slot,job);if(!continuation.ok)return {false,{},std::move(continuation.error)};
                auto producer=checkpoint_make_shared<CoreSlotProducer>(capture_);capsule_consumed_=true;
                auto frozen=freeze_slot_envelope(key.id,expected_+1,saved_,std::move(producer),std::move(continuation.value),std::move(capsule_),capacity,slot,job);if(!frozen.ok)return {false,{},std::move(frozen.error)};
                result->envelope=std::move(frozen.value);result->inactive=selected.value.present&&selected.value.bank==SlotBank::b?SlotBank::a:SlotBank::b;
            }else{
                if(!staged)return {false,{},job.capture_boundary_failure("StorageInvalidAnalysis",{},ErrorKind::rule)};
                auto read=inspect_slot_envelope(*staged,key.id,capacity,slot,job);if(!read.ok)return {false,{},std::move(read.error)};
                const auto& got=read.value.value();const auto& expected=frozen_->index();
                if(got.generation!=expected.generation||got.saved_at_unix_seconds!=expected.saved_at_unix_seconds||got.total_file_bytes!=expected.total_file_bytes)return {false,{},job.capture_boundary_failure("StorageReadbackMismatch",{},ErrorKind::rule)};
                for(std::size_t i=0;i<3;++i){const auto&a=got.payloads[i];const auto&b=expected.payloads[i];if(a.kind!=b.kind||a.present!=b.present||a.codec!=b.codec||a.offset!=b.offset||a.length!=b.length||a.sha256!=b.sha256)return {false,{},job.capture_boundary_failure("StorageReadbackMismatch",{},ErrorKind::rule)};}
                SlotPayloadReadSource checkpoint(*staged,got.payloads[0]);const auto& c=got.payloads[1];BoundContinuationSource continuation{c.present?staged:nullptr,c.offset,c.length,c.sha256};
                auto candidate=WorldRestoreCandidate::prepare(checkpoint,got.payloads[0].sha256,capture_->catalog(),continuation,capacity,slot,job);if(!candidate.ok)return {false,{},std::move(candidate.error)};
                const auto& source=candidate.value.source_identity();const auto& frozen_source=capture_->source_identity();
                if(source.world_id!=frozen_source.world_id||source.revision!=frozen_source.revision||source.tick!=frozen_source.tick)return {false,{},job.capture_boundary_failure("StorageSourceMismatch",{},ErrorKind::rule)};
                const auto&a=got.payloads[2];auto admission=decode_slot_admission_capsule(*staged,a.offset,a.length,a.sha256,capacity,slot,job);if(!admission.ok)return {false,{},std::move(admission.error)};
                auto baseline=candidate.value.verify_admission_baseline(admission.value.value().source_world_id,admission.value.value().baseline,capacity,job);if(!baseline.ok)return {false,{},std::move(baseline.error)};
                result->envelope=frozen_;result->staged=true;
            }
            if(cancel.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
            std::shared_ptr<const StorageStreamAnalysisResult> owned=std::move(result);return {true,std::move(owned),{}};
        }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
    }
private:std::shared_ptr<const WorldCheckpointCapture> capture_;mutable CheckpointJobBytes capsule_;mutable bool capsule_consumed_=false;Integer expected_=0;std::uint64_t saved_=0;std::shared_ptr<const FrozenSlotEnvelope> frozen_;
};
std::shared_ptr<const StoredSlotReceipt> prepare_receipt(const FrozenSlotEnvelope& envelope,const WorldCheckpointCapture& capture){
    const auto& index=envelope.index();auto charge=CheckpointMemoryCharge(0);
    StoredSlotReceiptValue value;value.slot_id=StableId(index.save_id);value.generation=index.generation;value.checkpoint_identity=CheckpointIdentity(capture.identity());value.payload_sha256=index.payloads[0].sha256;value.byte_length=index.payloads[0].length;value.saved_at=index.saved_at_unix_seconds;value.verification=StableId("readback_verified");value.admission_capsule_sha256=index.payloads[2].sha256;
    return checkpoint_make_shared<StoredSlotReceipt>(std::move(charge),std::move(value));
}
}
struct ResidentSlotCoordinator::PendingStream {
    StorageOperationRef reference;ResidentReplaySession* session=nullptr;Integer expected=0;SlotSaveState phase=SlotSaveState::inspecting;
    std::shared_ptr<CheckpointJobBudget> job;std::shared_ptr<const WorldCheckpointCapture> capture;std::shared_ptr<const SlotSaveAnalysis> analysis;
    std::shared_ptr<const FrozenSlotEnvelope> envelope;std::shared_ptr<const StoredSlotReceipt> prepared_receipt;
    CheckpointJobFailure failure;bool analysis_queued=false,publication_requested=false;StorageDiagnostic reject_code;
};
StorageAccepted ResidentSlotCoordinator::begin_save_v2(ResidentReplaySession& session,ResidentProfileCoordinator& profile,const std::string& slot,Integer generation){
    if(!owner_thread())return {false,{},"StorageWrongOwnerThread"};if(closing_)return {false,{},"StorageClosing"};if(pending_||pending_load_||pending_stream_)return {false,{},"StorageBusy"};if(!profile.owns_session(session))return {false,{},"StorageSessionMismatch"};if(!ResidentSlotMedia::valid_slot(slot))return {false,{},"InvalidSlot"};if(generation<0||generation==INT64_MAX)return {false,{},"InvalidGeneration"};
    std::shared_ptr<PendingStream> pending;
    try{
        const auto& capacity=engineering_checkpoint_capacity_profile();const auto& limits=engineering_slot_capacity_profile();auto made=CheckpointJobBudget::create(capacity,limits);if(!made.ok){return StorageAccepted::rejected(std::move(made.error));}
        auto resource=checkpoint_memory_resource(made.value,CheckpointJobUse::encode);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);
        pending=checkpoint_make_shared<PendingStream>();pending->job=checkpoint_make_shared<CheckpointJobBudget>(std::move(made.value));pending->session=&session;pending->expected=generation;
        const auto fail=[&](CheckpointJobFailure&& error){pending->failure=std::move(error);StorageAccepted result;result.job_failure=CheckpointJobFailureLease::retain(pending->failure);return result;};
        auto captured=session.capture_storage_checkpoint(capacity,limits,*pending->job);if(!captured.ok)return fail(std::move(captured.error));pending->capture=std::move(captured.value);
        auto admission=profile.capture_bytes(pending->capture->source_identity(),capacity,limits,*pending->job);if(!admission.ok)return fail(std::move(admission.error));if(!same(admission.value.source_identity,pending->capture->source_identity()))return fail(pending->job->capture_boundary_failure("StorageSourceMismatch",{},ErrorKind::rule));
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();if(seconds<0)return fail(pending->job->capture_boundary_failure("InvalidWallClock",{},ErrorKind::rule));
        pending->analysis=checkpoint_make_shared<SlotSaveAnalysis>(pending->capture,std::move(admission.value.capsule),generation,std::uint64_t(seconds));
        auto accepted=storage_.inspect_stream({slot,SlotFileKind::slot_envelope_v2},pending->capture->source_identity(),pending->job);if(!accepted.ok)return accepted;
        pending->reference=accepted.reference;pending_stream_=std::move(pending);cancelling_=false;{std::lock_guard<std::mutex> lock(mutex_);active_=accepted.reference;}publish_stream(SlotSaveState::inspecting);return accepted;
    }catch(checkpoint_job_detail::FailureException& e){if(pending){pending->failure=std::move(e.error);StorageAccepted result;result.job_failure=CheckpointJobFailureLease::retain(pending->failure);return result;}return StorageAccepted::rejected(std::move(e.error));}
    catch(const std::bad_alloc&){if(pending&&pending->job){pending->failure=pending->job->capture_fixed_failure(CheckpointJobFailureCode::allocation_failed);StorageAccepted result;result.job_failure=CheckpointJobFailureLease::retain(pending->failure);return result;}StorageAccepted result;result.fixed_failure=CheckpointJobFailureCode::allocation_failed;return result;}
}
// 只有真实worker终态durable后才把预备深值回执移入公开缓存。
// 只在底层正常取消或未发布完成时保留 owner 原拒绝；介质/清理失败不得覆盖。
void ResidentSlotCoordinator::publish_stream(SlotSaveState phase,
                                             const StoragePoll *transport) {
  auto &p = *pending_stream_;
  p.phase = phase;
  std::lock_guard<std::mutex> lock(mutex_);
  cached_.ok = true;
  cached_.reference = p.reference;
  cached_.generation = p.expected + 1;
  cached_.state = phase;
  if (transport) {
    cached_.code = transport->code;
    cached_.system_error = transport->system_error;
    cached_.durability = transport->durability;
    cached_.job_failure = transport->job_failure;
    if (phase == SlotSaveState::failed && !p.reject_code.empty() &&
        (transport->state == StorageState::cancelled ||
         (transport->state == StorageState::completed &&
          !p.publication_requested)))
      cached_.code = p.reject_code;
  }
  if (p.failure.code() != CheckpointJobFailureCode::none)
    cached_.job_failure = CheckpointJobFailureLease::retain(p.failure);
  if (phase == SlotSaveState::completed && transport &&
      transport->durability == SlotDurability::durable_confirmed)
    cached_.stored = p.prepared_receipt;
}
StorageStatus ResidentSlotCoordinator::advance_stream_save(ResidentReplaySession& session){
    if(!owner_thread())return {false,"StorageWrongOwnerThread"};auto& p=*pending_stream_;if(p.session!=&session)return {false,"StorageSessionMismatch"};if(terminal(p.phase))return {true,{}};
    const auto reject=[&](const StorageDiagnostic& code){p.reject_code=code;storage_.cancel(p.reference);publish_stream(SlotSaveState::cancelling);};
    try{auto result=storage_.poll(p.reference);if(!result.ok)return {false,result.code};if(result.state==StorageState::queued||result.state==StorageState::running)return {true,{}};
        if(result.state==StorageState::awaiting_recovery){publish_stream(SlotSaveState::awaiting_recovery,&result);return {true,{}};}
        if(result.state==StorageState::failed||result.state==StorageState::cancelled||result.state==StorageState::closed_unconfirmed){publish_stream(result.state==StorageState::cancelled&&p.reject_code.empty()&&p.failure.code()==CheckpointJobFailureCode::none?SlotSaveState::cancelled:result.state==StorageState::closed_unconfirmed?SlotSaveState::closed_unconfirmed:SlotSaveState::failed,&result);return {true,{}};}
        if(p.publication_requested&&result.state==StorageState::completed){publish_stream(SlotSaveState::completed,&result);return {true,{}};}
        if(p.phase==SlotSaveState::cancelling){publish_stream(p.reject_code.empty()&&p.failure.code()==CheckpointJobFailureCode::none?SlotSaveState::cancelled:SlotSaveState::failed,&result);return {true,{}};}
        if(cancelling_||closing_){reject({});return {true,{}};}
        auto current=session.identity();const auto& source=p.reference.source;if(!current.ok||current.value.world_id!=source.world_id||current.value.execution_epoch!=source.execution_epoch||current.value.revision<source.revision||current.value.tick<source.tick){reject("StaleWorld");return {true,{}};}
        if(p.phase==SlotSaveState::inspecting){auto accepted=storage_.analyze_stream(p.reference,0,p.analysis);if(!accepted.ok){reject(accepted.code);return accepted;}publish_stream(SlotSaveState::awaiting_preflight);return {true,{}};}
        if(p.phase==SlotSaveState::awaiting_preflight){auto analysis=std::dynamic_pointer_cast<const SaveAnalysisResult>(result.stream_analysis);if(!analysis||analysis->staged||!analysis->envelope){reject("StorageInvalidAnalysisResult");return {true,{}};}p.envelope=analysis->envelope;auto queued=storage_.continue_stream_stage(p.reference,p.expected,analysis->inactive,p.envelope);if(!queued.ok){reject(queued.code);return queued;}p.analysis.reset();publish_stream(SlotSaveState::staging);return {true,{}};}
        if(p.phase==SlotSaveState::staging&&result.state==StorageState::awaiting_owner_validation){auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::encode);if(!resource.ok)throw checkpoint_job_detail::FailureException{std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);p.analysis=checkpoint_make_shared<SlotSaveAnalysis>(p.capture,p.envelope);auto queued=storage_.analyze_stream(p.reference,result.stage_token,p.analysis);if(!queued.ok){reject(queued.code);return queued;}publish_stream(SlotSaveState::awaiting_validation);return {true,{}};}
        if(p.phase==SlotSaveState::awaiting_validation&&result.state==StorageState::awaiting_owner_validation){auto proof=std::dynamic_pointer_cast<const SaveAnalysisResult>(result.stream_analysis);if(!proof||!proof->staged||proof->envelope!=p.envelope){reject("StorageInvalidAnalysisResult");return {true,{}};}auto resource=checkpoint_memory_resource(*p.job,CheckpointJobUse::encode);if(!resource.ok)throw checkpoint_job_detail::FailureException{std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);p.prepared_receipt=prepare_receipt(*p.envelope,*p.capture);if(cancelling_||closing_){reject({});return {true,{}};}auto promoted=storage_.promote_after_owner_validation(p.reference,result.stage_token);if(!promoted.ok){reject(promoted.code);return promoted;}p.publication_requested=true;publish_stream(SlotSaveState::promoting);return {true,{}};}
        return {true,{}};
    }catch(checkpoint_job_detail::FailureException& e){p.failure=std::move(e.error);reject({});return {true,{}};}catch(const std::bad_alloc&){p.failure=p.job->capture_fixed_failure(CheckpointJobFailureCode::allocation_failed);storage_.cancel(p.reference);publish_stream(SlotSaveState::cancelling);return {true,{}};}
}
StorageStatus ResidentSlotCoordinator::recover_stream_save(const StorageOperationRef& reference){if(!owner_thread())return {false,"StorageWrongOwnerThread"};auto&p=*pending_stream_;if(!same(reference,p.reference))return {false,"StorageStaleReference"};if(p.phase!=SlotSaveState::awaiting_recovery)return {false,"StorageInvalidStage"};auto result=storage_.poll(reference);if(!result.ok)return {false,result.code};auto recovered=storage_.recover_same_bytes(reference,result.stage_token);if(recovered.ok)publish_stream(SlotSaveState::promoting);return recovered;}
StorageStatus ResidentSlotCoordinator::release_stream_save(const StorageOperationRef& reference){if(!owner_thread())return {false,"StorageWrongOwnerThread"};if(!same(reference,pending_stream_->reference))return {false,"StorageStaleReference"};if(!terminal(pending_stream_->phase))return {false,"StorageOperationPending"};auto released=storage_.release(reference);if(!released.ok)return released;{std::lock_guard<std::mutex> lock(mutex_);cached_={};active_.reset();}pending_stream_.reset();return {true,{}};}
}
