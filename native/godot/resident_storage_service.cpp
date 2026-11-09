// 槽服务把既有字节的I/O和阶段生命周期放在独立线程；语义核验往返仍由原协调者负责。
#include "resident_storage_service.hpp"
#include "resident_profile_admission.hpp"
#include "resident_slot_envelope.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <limits>
#include <string_view>
#include <type_traits>

namespace hanmo::resident::storage {
namespace {
// 实例号只用于拒绝旧运输引用，不写进世界或物理槽。
std::atomic<std::uint64_t> service_instances{0};
// std::runtime_error owns an immutable diagnostic. Its noexcept copy preserves the exact original
// code while a worker publishes failure under exhausted allocation capacity.
static_assert(std::is_nothrow_copy_constructible_v<SlotMediaError>);
static_assert(std::is_nothrow_move_constructible_v<SlotMediaError>);
static_assert(std::is_nothrow_move_assignable_v<std::optional<SlotMediaError>>);
// 票据核完整捕获来源；当前世界换epoch不会改写旧任务的真实磁盘结果。
bool same_source(const WorldIdentity& left,const WorldIdentity& right){return left.world_id==right.world_id&&left.execution_epoch==right.execution_epoch&&left.revision==right.revision&&left.tick==right.tick;}
// 介质终态与业务成功分开；unknown仍占槽，不能释放后假装另存新代解决。
bool terminal(StorageState state){return state==StorageState::completed||state==StorageState::cancelled||state==StorageState::failed||state==StorageState::closed_unconfirmed;}
// 文件及观察值都是深拥有输入；预算加法先检查，未知大小不允许隐式无限排队。
std::optional<std::size_t> owned_size(const SlotWriteRequest& request){
    std::size_t total=request.bytes.size();for(const auto* image:{&request.expected.a,&request.expected.b}){if(image->bytes.size()>std::numeric_limits<std::size_t>::max()-total)return {};total+=image->bytes.size();}return total;
}
}
// 关闭服务的纯内存字节账，原 lease 仍随最后一份不可变读回结果存活；操作数/同槽/文件/磁盘门另管。
struct ResidentStorageService::Budget {
    static constexpr bool metered=false; // 本批生产固定关闭，不是作者或存档开关。
    std::atomic<std::size_t> bytes{0};std::size_t maximum;
    explicit Budget(std::size_t limit):maximum(limit){}
    bool reserve(std::size_t amount){if(!metered)return true;auto current=bytes.load();do{if(current>maximum||amount>maximum-current)return false;}while(!bytes.compare_exchange_weak(current,current+amount));return true;}
    void give_back(std::size_t amount){if(metered)bytes.fetch_sub(amount);}
    // 共享租约的控制块分配失败时尚未构造Lease，调用方仅在该情形归还预留。
    struct Lease {
        std::shared_ptr<Budget> owner;std::size_t amount;
        Lease(std::shared_ptr<Budget> budget,std::size_t bytes,bool* constructed=nullptr):owner(std::move(budget)),amount(bytes){if(constructed)*constructed=true;}
        ~Lease(){owner->give_back(amount);}
    };
    // 对象与shared control分两次真实分配；后者失败会调用Lease析构，不能重复退服务配额。
    static std::shared_ptr<Lease> own_reserved(const std::shared_ptr<Budget>& owner,std::size_t amount){bool constructed=false;try{return checkpoint_make_shared<Lease>(owner,amount,&constructed);}catch(...){if(!constructed)owner->give_back(amount);throw;}}

};
// record只存运输和介质阶段，不持Node、Callable、WorldBundle或可变领域引用。
struct ResidentStorageService::Record {
    CheckpointJobReservation job_control;
    Record* queued_next=nullptr;JobKind queued_kind=JobKind::inspect;bool queued=false;
    std::shared_ptr<CheckpointMemoryResource> worker_memory;
    CheckpointMemoryCharge owned_memory;
    std::string import_path;CheckpointDigest import_sha256{};
    StorageOperationRef reference;SlotKey slot;StorageState state=StorageState::queued;
    SlotDurability durability=SlotDurability::not_published;const char* literal_code="";
    std::optional<SlotMediaError> media_error;int system_error=0;std::uint64_t stage_token=0;
    const char* code()const noexcept{return media_error?media_error->what():literal_code;}
    SlotCancellation cancellation;std::shared_ptr<Budget::Lease> allocation;
    std::shared_ptr<const SlotWriteRequest> write;std::shared_ptr<const SlotBankPair> banks;std::shared_ptr<const JobSlotBankPair> job_banks;
    std::shared_ptr<PreparedSlotWrite> prepared;
    std::shared_ptr<CheckpointJobBudget> job;
    std::shared_ptr<const SlotStreamWriteRequest> stream;
    std::shared_ptr<const SlotStreamBankPair> stream_banks;
    CheckpointJobFailure job_failure;
    std::shared_ptr<const StorageStreamAnalysis> analysis;
    std::shared_ptr<const StorageStreamAnalysisResult> analysis_result;
    bool analysis_is_stage=false,retiring=false;
    std::shared_ptr<const FrozenSlotEnvelope> next_envelope;SlotBank next_inactive=SlotBank::b;Integer next_expected=0;
};
// 构造只核有限配置并启动线程，不在调用线程创建目录或执行文件I/O。
ResidentStorageService::ResidentStorageService(std::string directory,StorageServiceLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
,SlotIoTestHooks hooks
#endif
):directory_(std::move(directory)),limits_(limits),budget_(checkpoint_make_shared<Budget>(limits.max_owned_bytes))
#ifdef HANMO_STORAGE_TEST_FAULTS
,hooks_(std::move(hooks))
#endif
{
    const auto maximum=static_cast<std::uint64_t>(std::numeric_limits<Integer>::max());
    if(limits.max_operations==0||limits.max_owned_bytes==0||limits.media.max_file_bytes==0||limits.media.max_read_pair_bytes==0||limits.media.max_staging_disk_bytes==0||limits.max_operations>maximum||limits.max_owned_bytes>maximum||limits.media.max_file_bytes>maximum||limits.media.max_read_pair_bytes>maximum||limits.media.max_staging_disk_bytes>maximum)throw SlotMediaError("StorageInvalidConfiguration");
    instance_=++service_instances;if(instance_==0||instance_>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))throw SlotMediaError("StorageOperationLimit");
    try{worker_=std::thread([this]{work();});}catch(...){throw SlotMediaError("StorageWorkerStartFailed");}
}
// 非阻塞退出必须先走request_close/close_complete；析构join仅保最后的内存安全兜底。
ResidentStorageService::~ResidentStorageService(){request_close();if(worker_.joinable())worker_.join();}
// 只在mutex内查询记录；不借其它operation或新epoch的完成值补旧请求。
std::shared_ptr<ResidentStorageService::Record> ResidentStorageService::find(const StorageOperationRef& reference)const{
    if(reference.instance!=instance_)return {};
    const auto found=records_.find(reference.operation);
    if(found==records_.end()||!same_source(reference.source,found->second->reference.source))return {};
    return found->second;
}
// These two helpers require mutex_. The index owns every queued record, including dispose.
// Cancelled staged cleanup keeps its original priority over this FIFO.
bool ResidentStorageService::enqueue(Record& record,JobKind kind)noexcept{
    if(record.queued||record.retiring)return false;
    record.queued=true;record.queued_kind=kind;record.queued_next=nullptr;
    if(queue_tail_)queue_tail_->queued_next=&record;else queue_head_=&record;
    queue_tail_=&record;return true;
}
std::shared_ptr<ResidentStorageService::Record> ResidentStorageService::dequeue(Job& job)noexcept{
    if(!queue_head_)return {};
    Record* record=queue_head_;queue_head_=record->queued_next;if(!queue_head_)queue_tail_=nullptr;
    record->queued_next=nullptr;record->queued=false;job={record->reference.operation,record->queued_kind};
    const auto found=records_.find(job.id);return found==records_.end()?std::shared_ptr<Record>{}:found->second;
}
// 接纳时同时冻结操作数和字节租约；拒绝不产生假完成，不覆盖已有写请求。
StorageAccepted ResidentStorageService::accept(std::shared_ptr<Record> record,JobKind kind){
    std::lock_guard<std::mutex> lock(mutex_);
    if(closing_)return {false,{},"StorageClosing"};
    if(startup_message()[0])return {false,{},startup_error_?StorageDiagnostic::from_media(*startup_error_):StorageDiagnostic(startup_code_)};
    if(records_.size()>=limits_.max_operations)return {false,{},"StorageBusy"};
    if(record->write||record->stream){for(const auto& entry:records_){const auto& previous=*entry.second;if((previous.write||previous.stream)&&!terminal(previous.state)&&previous.slot.id==record->slot.id&&previous.slot.kind==record->slot.kind)return {false,{},"StorageSlotBusy"};}}
    if(next_operation_>=static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))return {false,{},"StorageOperationLimit"};
    // 原首错的零额身份随 record 保留；清理失败只变介质诊断，不丢同 job 首错。
    if(record->job){auto held=record->job->reserve(CheckpointJobUse::slot,0);if(!held.ok)return StorageAccepted::rejected(std::move(held.error));record->job_control=std::move(held.value);}
    record->reference.instance=instance_;record->reference.operation=++next_operation_;const auto id=record->reference.operation;
    try{ScopedCheckpointMemory scope(record->worker_memory);records_.emplace(id,record);}
    catch(checkpoint_job_detail::FailureException& error){return StorageAccepted::rejected(std::move(error.error));}
    catch(const std::bad_alloc&){return record->job?StorageAccepted::rejected(record->job->capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)):StorageAccepted{false,{},"StorageResourceLimit"};}
    if(!enqueue(*record,kind)){records_.erase(id);return {false,{},"StorageInvalidStage"};}
    wake_.notify_one();return {true,record->reference,{}};
}
// 只读请求先预留两银行的最大内存，实际读取仍由唯一worker负责。
StorageAccepted ResidentStorageService::inspect(const SlotKey& key,const WorldIdentity& source){
    if(!ResidentSlotMedia::valid_key(key))return {false,{},"InvalidSlot"};
    if(key.profile_namespace_binding!=limits_.media.host_profile_namespace_binding)return {false,{},"StorageTargetMismatch"};
    const auto charge=limits_.media.max_read_pair_bytes;if(!budget_->reserve(charge))return {false,{},"StorageByteBudget"};
    std::shared_ptr<Budget::Lease> allocation;try{allocation=std::make_shared<Budget::Lease>(budget_,charge);}
    catch(const std::bad_alloc&){budget_->give_back(charge);return {false,{},"StorageResourceLimit"};}
    try{auto record=std::make_shared<Record>();record->allocation=std::move(allocation);record->slot=key;record->reference.source=source;return accept(std::move(record),JobKind::inspect);}
    catch(const std::bad_alloc&){return {false,{},"StorageResourceLimit"};}
}
// 接纳前深冻既有封套与原观察值，返回票据并不等于语义或介质成功。
StorageAccepted ResidentStorageService::stage(const SlotWriteRequest& input){
    if(!ResidentSlotMedia::valid_key(input.slot))return {false,{},"InvalidSlot"};
    if(input.slot.profile_namespace_binding!=limits_.media.host_profile_namespace_binding)return {false,{},"StorageTargetMismatch"};
    if(input.expected_generation<0||input.expected_generation==std::numeric_limits<Integer>::max()||input.new_generation!=input.expected_generation+1)return {false,{},"InvalidGeneration"};
    const auto charge=owned_size(input);if(!charge||input.bytes.empty()||input.bytes.size()>limits_.media.max_file_bytes||input.expected.a.bytes.size()>limits_.media.max_file_bytes||input.expected.b.bytes.size()>limits_.media.max_file_bytes)return {false,{},"StorageCapacityExceeded"};
    if(!budget_->reserve(*charge))return {false,{},"StorageByteBudget"};
    std::shared_ptr<Budget::Lease> allocation;try{allocation=std::make_shared<Budget::Lease>(budget_,*charge);}
    catch(const std::bad_alloc&){budget_->give_back(*charge);return {false,{},"StorageResourceLimit"};}
    try{
        auto record=std::make_shared<Record>();record->allocation=allocation;record->slot=input.slot;record->reference.source=input.source;
        // 立即复制调用者vector，不能把可变别名或Godot packed数组交worker。
        record->write=std::shared_ptr<const SlotWriteRequest>(new SlotWriteRequest(input),[allocation](const SlotWriteRequest* value){delete value;});
        return accept(std::move(record),JobKind::stage);
    }catch(const std::bad_alloc&){return {false,{},"StorageResourceLimit"};}
}
// META使用原Bytes介质但显式沿同capture job：冻结副本的租约跟随最后一个只读结果。
StorageAccepted ResidentStorageService::inspect_profile_metadata(const SlotKey& key,const WorldIdentity& source,std::shared_ptr<CheckpointJobBudget> job){
    if(!job||key.kind!=SlotFileKind::profile_metadata||!ResidentSlotMedia::valid_key(key)||key.profile_namespace_binding!=limits_.media.host_profile_namespace_binding)return {false,{},"StorageInvalidRequest"};
    try{auto resource=checkpoint_memory_resource(*job,CheckpointJobUse::candidate);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);
        const auto amount=limits_.media.max_read_pair_bytes;if(!budget_->reserve(amount))return {false,{},"StorageByteBudget"};auto allocation=Budget::own_reserved(budget_,amount);
        auto record=checkpoint_make_shared<Record>();record->allocation=std::move(allocation);record->owned_memory=CheckpointMemoryCharge(0);record->slot=SlotKey{StableId(key.id),key.kind,StableId(key.profile_namespace_binding)};record->worker_memory=resource.value;record->job=std::move(job);record->reference.source=source;return accept(std::move(record),JobKind::inspect);
    }catch(checkpoint_job_detail::FailureException& e){return StorageAccepted::rejected(std::move(e.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
StorageAccepted ResidentStorageService::stage_profile_metadata(const SlotWriteRequest& input,std::shared_ptr<CheckpointJobBudget> job){
    if(!job||input.slot.kind!=SlotFileKind::profile_metadata||!ResidentSlotMedia::valid_key(input.slot)||input.slot.profile_namespace_binding!=limits_.media.host_profile_namespace_binding)return {false,{},"StorageInvalidRequest"};
    if(input.expected_generation<0||input.expected_generation==INT64_MAX||input.new_generation!=input.expected_generation+1)return {false,{},"InvalidGeneration"};
    auto amount=owned_size(input);if(!amount||input.bytes.empty()||input.bytes.size()>limits_.media.max_file_bytes||input.expected.a.bytes.size()>limits_.media.max_file_bytes||input.expected.b.bytes.size()>limits_.media.max_file_bytes)return {false,{},"StorageCapacityExceeded"};
    try{auto resource=checkpoint_memory_resource(*job,CheckpointJobUse::candidate);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);
        if(!budget_->reserve(*amount))return {false,{},"StorageByteBudget"};auto allocation=Budget::own_reserved(budget_,*amount);
        struct Frozen{CheckpointMemoryCharge memory;std::shared_ptr<void> limit;SlotWriteRequest value;Frozen(CheckpointMemoryCharge&& charge,std::shared_ptr<void> lease,const SlotWriteRequest& source):memory(std::move(charge)),limit(std::move(lease)),value(source){}};
        auto frozen=checkpoint_make_shared<Frozen>(CheckpointMemoryCharge(0),allocation,input);
        auto record=checkpoint_make_shared<Record>();record->owned_memory=CheckpointMemoryCharge(0);record->slot=SlotKey{StableId(input.slot.id),input.slot.kind,StableId(input.slot.profile_namespace_binding)};record->reference.source=input.source;record->worker_memory=resource.value;record->job=std::move(job);record->write=std::shared_ptr<const SlotWriteRequest>(frozen,&frozen->value);record->allocation=std::move(allocation);return accept(std::move(record),JobKind::stage);
    }catch(checkpoint_job_detail::FailureException& e){return StorageAccepted::rejected(std::move(e.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
// 流请求在接纳前保留同job控制分配，worker读取真实fd而不为两银行预分配整档。
StorageAccepted ResidentStorageService::inspect_stream(const SlotKey& key,const WorldIdentity& source,std::shared_ptr<CheckpointJobBudget> job){
    if(!job||key.kind!=SlotFileKind::slot_envelope_v2||!ResidentSlotMedia::valid_key(key)||!limits_.media.host_profile_namespace_binding.empty())return {false,{},"StorageInvalidRequest"};
    try{auto resource=checkpoint_memory_resource(*job,CheckpointJobUse::slot);if(!resource.ok)return StorageAccepted::rejected(std::move(resource.error));ScopedCheckpointMemory scope(resource.value);
        auto charge=CheckpointMemoryCharge(0);auto record=checkpoint_make_shared<Record>();record->owned_memory=std::move(charge);record->worker_memory=resource.value;record->job=std::move(job);record->slot=SlotKey{StableId(key.id),key.kind,StableId(key.profile_namespace_binding)};record->reference.source=source;return accept(std::move(record),JobKind::inspect_stream);
    }catch(checkpoint_job_detail::FailureException& error){return StorageAccepted::rejected(std::move(error.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
// 只有原profile协调者能提交其真实create后封闭捕获；无银行/临时文件的纯分析不授发布权。
StorageAccepted ResidentStorageService::analyze_profile_capture(const SlotKey& key,const WorldIdentity& source,std::shared_ptr<CheckpointJobBudget> job,std::shared_ptr<const StorageStreamAnalysis> analysis){
    if(!job||!analysis||key.kind!=SlotFileKind::profile_metadata||!ResidentSlotMedia::valid_key(key)||key.profile_namespace_binding!=limits_.media.host_profile_namespace_binding)return {false,{},"StorageInvalidRequest"};
    try{auto resource=checkpoint_memory_resource(*job,CheckpointJobUse::slot);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);
        auto held=CheckpointMemoryCharge(0);auto record=checkpoint_make_shared<Record>();record->owned_memory=std::move(held);record->slot=SlotKey{StableId(key.id),key.kind,StableId(key.profile_namespace_binding)};record->reference.source=source;record->worker_memory=resource.value;record->job=std::move(job);record->analysis=std::move(analysis);return accept(std::move(record),JobKind::analyze_stream);
    }catch(checkpoint_job_detail::FailureException& error){return StorageAccepted::rejected(std::move(error.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
// Prepare/Accept复用同record、同路径和全档摘要；重新打开实际文件，不能凭旧preview跳过冷核。
StorageAccepted ResidentStorageService::analyze_import_file(const std::string& path,const CheckpointDigest& digest,const WorldIdentity& source,std::shared_ptr<CheckpointJobBudget> job,std::shared_ptr<const StorageStreamAnalysis> analysis){
    if(!job||!analysis||path.empty()||path.size()>4096||path.front()!='/'||path.find('\0')!=std::string::npos||limits_.media.host_profile_namespace_binding.empty())return {false,{},"StorageInvalidImportRequest"};
    try{auto resource=checkpoint_memory_resource(*job,CheckpointJobUse::slot);if(!resource.ok){return StorageAccepted::rejected(std::move(resource.error));}ScopedCheckpointMemory scope(resource.value);auto charge=CheckpointMemoryCharge(0);auto record=checkpoint_make_shared<Record>();record->owned_memory=std::move(charge);record->import_path=StableId(path);record->import_sha256=digest;record->slot={"profile_metadata",SlotFileKind::profile_metadata,StableId(limits_.media.host_profile_namespace_binding)};record->reference.source=source;record->worker_memory=resource.value;record->job=std::move(job);record->analysis=std::move(analysis);return accept(std::move(record),JobKind::analyze_import);
    }catch(checkpoint_job_detail::FailureException& e){return StorageAccepted::rejected(std::move(e.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
StorageStatus ResidentStorageService::revalidate_import_file(const StorageOperationRef& reference,std::shared_ptr<const StorageStreamAnalysis> analysis){
    std::lock_guard<std::mutex> lock(mutex_);auto record=find(reference);if(!record||record->retiring)return {false,"StorageStaleReference"};if(closing_)return {false,"StorageClosing"};if(!analysis||record->import_path.empty()||record->state!=StorageState::completed||!record->analysis_result||record->analysis)return {false,"StorageInvalidImportStage"};if(record->cancellation.cancelled())return {false,"StorageCancelled"};
    if(!enqueue(*record,JobKind::analyze_import))return {false,"StorageInvalidStage"};record->analysis=std::move(analysis);record->analysis_result.reset();record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 只浅共享拥有且不可变的producer/观察lease，标量请求本身深冻结，后续不能换槽换source。
StorageAccepted ResidentStorageService::stage_stream(const SlotStreamWriteRequest& input){
    if(!input.job||!input.envelope||!input.expected||input.slot.kind!=SlotFileKind::slot_envelope_v2||!ResidentSlotMedia::valid_key(input.slot)||!limits_.media.host_profile_namespace_binding.empty())return {false,{},"StorageInvalidRequest"};
    try{auto resource=checkpoint_memory_resource(*input.job,CheckpointJobUse::slot);if(!resource.ok)return StorageAccepted::rejected(std::move(resource.error));ScopedCheckpointMemory scope(resource.value);
        struct Frozen {CheckpointMemoryCharge memory;SlotStreamWriteRequest value;Frozen(CheckpointMemoryCharge&& held,const SlotStreamWriteRequest& original):memory(std::move(held)),value(original){}};
        auto frozen=checkpoint_make_shared<Frozen>(CheckpointMemoryCharge(0),input);
        auto record=checkpoint_make_shared<Record>();record->owned_memory=CheckpointMemoryCharge(0);record->worker_memory=resource.value;record->job=input.job;record->slot=SlotKey{StableId(input.slot.id),input.slot.kind,StableId(input.slot.profile_namespace_binding)};record->reference.source=input.source;record->stream=std::shared_ptr<const SlotStreamWriteRequest>(frozen,&frozen->value);return accept(std::move(record),JobKind::stage_stream);
    }catch(checkpoint_job_detail::FailureException& error){return StorageAccepted::rejected(std::move(error.error));}catch(const std::bad_alloc&){return StorageAccepted::rejected(CheckpointJobFailure::fixed(CheckpointJobFailureCode::allocation_failed));}
}
// 有限轮询只共享不可变结果，核完整来源后取自己的实际阶段，不等待文件或世界。
StoragePoll ResidentStorageService::poll(const StorageOperationRef& reference)const{
    std::lock_guard<std::mutex> lock(mutex_);const auto record=find(reference);if(!record||record->retiring){StoragePoll rejected;rejected.code="StorageStaleReference";return rejected;}
    StoragePoll result;result.ok=true;result.code=record->media_error?StorageDiagnostic::from_media(*record->media_error):StorageDiagnostic(record->literal_code);result.state=record->state;result.durability=record->durability;result.system_error=record->system_error;result.stage_token=record->stage_token;result.frozen_write=record->write;result.banks=record->banks;result.job_banks=record->job_banks;result.stream_analysis=record->analysis_result;if(record->job_failure.code()!=CheckpointJobFailureCode::none)result.job_failure=CheckpointJobFailureLease::retain(record->job_failure);else result.job_failure=record->job_control.failure_lease();return result;
}
// 已完成inspect或原tmp读回阶段可排一次固定分析；任务接纳与close/cancel使用同锁。
StorageStatus ResidentStorageService::analyze_stream(const StorageOperationRef& reference,std::uint64_t token,std::shared_ptr<const StorageStreamAnalysis> analysis){
    std::lock_guard<std::mutex> lock(mutex_);auto record=find(reference);if(!record)return {false,"StorageStaleReference"};
    if(closing_)return {false,"StorageClosing"};if(!analysis||!record->job||record->slot.kind!=SlotFileKind::slot_envelope_v2)return {false,"StorageInvalidAnalysis"};
    const bool stage=record->prepared&&record->state==StorageState::awaiting_owner_validation&&token&&token==record->stage_token;
    const bool inspected=!record->prepared&&record->state==StorageState::completed&&record->stream_banks&&token==0;
    if((!stage&&!inspected)||record->analysis||record->analysis_result)return {false,"StorageInvalidStage"};
    if(record->cancellation.cancelled())return {false,"StorageCancelled"};
    if(!enqueue(*record,JobKind::analyze_stream))return {false,"StorageInvalidStage"};
    record->analysis=std::move(analysis);record->analysis_is_stage=stage;record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 原bank观察与来源留在同record；下一请求只能在其实际分析完成后冻结原CAS。
StorageStatus ResidentStorageService::continue_stream_stage(const StorageOperationRef& reference,Integer expected,SlotBank inactive,std::shared_ptr<const FrozenSlotEnvelope> envelope){
    std::lock_guard<std::mutex> lock(mutex_);auto record=find(reference);if(!record||record->retiring)return {false,"StorageStaleReference"};
    if(closing_)return {false,"StorageClosing"};if(record->cancellation.cancelled())return {false,"StorageCancelled"};
    if(!record->job||!record->stream_banks||!record->analysis_result||record->prepared||record->state!=StorageState::completed||!envelope||!envelope->belongs_to(*record->job)||expected<0||expected==INT64_MAX||envelope->index().generation!=expected+1||envelope->index().save_id!=record->slot.id)return {false,"StorageInvalidStage"};
    if(!enqueue(*record,JobKind::stage_stream))return {false,"StorageInvalidStage"};
    record->next_envelope=std::move(envelope);record->next_inactive=inactive;record->next_expected=expected;record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 既有核验消费者只能用原票据原token继续，不存在可传替换包或通用成功bool的入口。
StorageStatus ResidentStorageService::promote_after_owner_validation(const StorageOperationRef& reference,std::uint64_t token){
    std::lock_guard<std::mutex> lock(mutex_);const auto record=find(reference);if(!record)return {false,"StorageStaleReference"};
    if(record->slot.kind==SlotFileKind::profile_metadata)return {false,"ProfileValidatedStageRequired"};
    if(closing_)return {false,"StorageClosing"};
    if(record->state!=StorageState::awaiting_owner_validation||token==0||token!=record->stage_token)return {false,"StorageInvalidStage"};
    if(record->stream&&!record->analysis_result)return {false,"StorageStreamValidationRequired"};
    if(record->cancellation.cancelled())return {false,"StorageCancelled"};
    if(!enqueue(*record,JobKind::promote))return {false,"StorageInvalidStage"};
    record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 只认可实际原创建协调者核过的同一冻结对象；移动能力不能改绑另一目标或世界提升入口。
StorageStatus ResidentStorageService::promote_profile_metadata(const StorageOperationRef& reference,ProfileValidatedStage&& proof){
    std::lock_guard<std::mutex> lock(mutex_);const auto record=find(reference);if(!record)return {false,"StorageStaleReference"};
    if(closing_)return {false,"StorageClosing"};
    if(proof.consumed_||!proof.frozen_||record->slot.kind!=SlotFileKind::profile_metadata||record->write!=proof.frozen_||proof.instance_!=reference.instance||proof.operation_!=reference.operation||proof.stage_==0||proof.stage_!=record->stage_token||record->state!=StorageState::awaiting_owner_validation)return {false,"StorageInvalidProfileStage"};
    const auto& frozen=*record->write;
    if(proof.admission_id_.empty()||proof.binding_!=limits_.media.host_profile_namespace_binding||proof.binding_!=frozen.slot.profile_namespace_binding||proof.expected_generation_!=frozen.expected_generation||proof.new_generation_!=frozen.new_generation)return {false,"StorageProfileProofMismatch"};
    if(record->cancellation.cancelled())return {false,"StorageCancelled"};
    if(!enqueue(*record,JobKind::promote))return {false,"StorageInvalidStage"};
    proof.consumed_=true;record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 补屏障仅适用于本实例保留的同一已发布阶段，关闭后保明确未知终态而不伪恢复。
StorageStatus ResidentStorageService::recover_same_bytes(const StorageOperationRef& reference,std::uint64_t token){
    std::lock_guard<std::mutex> lock(mutex_);const auto record=find(reference);if(!record)return {false,"StorageStaleReference"};
    if(closing_)return {false,"StorageClosing"};
    if(record->state!=StorageState::awaiting_recovery||token==0||token!=record->stage_token)return {false,"StorageInvalidStage"};
    if(!enqueue(*record,JobKind::recover))return {false,"StorageInvalidStage"};
    record->state=StorageState::queued;wake_.notify_one();return {true,{}};
}
// 取消只提交原子请求，unlink仍由worker办理，发布已经开始则明确拒绝撤销。
StorageStatus ResidentStorageService::cancel(const StorageOperationRef& reference){
    std::lock_guard<std::mutex> lock(mutex_);const auto record=find(reference);if(!record)return {false,"StorageStaleReference"};
    if(terminal(record->state))return {false,"StorageAlreadyCompleted"};
    if(!record->cancellation.request())return {false,"StoragePublicationStarted"};
    if(record->job)record->job->cancel();
    wake_.notify_one();return {true,{}};
}
// 只能释放可领取终态，外部仍持有的深字节继续保有内存预算租约。
StorageStatus ResidentStorageService::release(const StorageOperationRef& reference){
    std::lock_guard<std::mutex> lock(mutex_);auto retired=find(reference);if(!retired||retired->retiring)return {false,"StorageStaleReference"};
    if(!terminal(retired->state)||retired->queued)return {false,"StorageOperationPending"};
    // close ack前worker已清完所有fd；正常释放也排原worker，poll的失败租约不携文件句柄。
    if(closed_||(!retired->prepared&&!retired->stream&&!retired->stream_banks)){records_.erase(reference.operation);return {true,{}};}
    if(!enqueue(*retired,JobKind::dispose))return {false,"StorageInvalidStage"};
    retired->retiring=true;wake_.notify_one();return {true,{}};
}
// 立即停止准入并请求取消；调用线程不join、不执行任何磁盘清理。
void ResidentStorageService::request_close(){
    std::lock_guard<std::mutex> lock(mutex_);closing_=true;
    for(const auto& entry:records_)if(!terminal(entry.second->state)&&entry.second->cancellation.request()){if(entry.second->job)entry.second->job->cancel();}
    wake_.notify_one();
}
// ack表示worker已完成所有接受请求的准确终结及句柄释放，原unknown/清理失败仍需读取。
bool ResidentStorageService::close_complete()const{std::lock_guard<std::mutex> lock(mutex_);return closed_;}
// 唯一worker管理目录锁、临时句柄与真实I/O；任务运行期间不持服务mutex。
void ResidentStorageService::work(){
    std::unique_ptr<ResidentSlotMedia> media;
    try{media=std::make_unique<ResidentSlotMedia>(directory_,limits_.media
#ifdef HANMO_STORAGE_TEST_FAULTS
        ,hooks_
#endif
    );}catch(const SlotMediaError& error){std::lock_guard<std::mutex> lock(mutex_);startup_error_.emplace(error);startup_system_error_=error.system_error();}
    catch(...){std::lock_guard<std::mutex> lock(mutex_);startup_code_="StorageOpenFailed";}
    for(;;){
        Job job{};std::shared_ptr<Record> record;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 等待原核验时被取消的tmp也由worker清理，cancel/close主线程不做unlink。
            auto cancelled_stage=[&]()->std::shared_ptr<Record>{for(const auto& entry:records_)if(entry.second->state==StorageState::awaiting_owner_validation&&entry.second->cancellation.cancelled())return entry.second;return {};};
            wake_.wait(lock,[&]{return closing_||queue_head_!=nullptr||cancelled_stage()!=nullptr;});
            if((record=cancelled_stage()))job={record->reference.operation,JobKind::cleanup};
            else if(queue_head_){record=dequeue(job);if(!record)continue;}
            else if(closing_)break;else continue;
            record->state=StorageState::running;
        }
        if(job.kind==JobKind::dispose){
            record->prepared.reset();record->stream.reset();record->stream_banks.reset();record->next_envelope.reset();record->analysis.reset();record->analysis_result.reset();
            std::lock_guard<std::mutex> lock(mutex_);records_.erase(job.id);continue;
        }
        std::shared_ptr<const SlotBankPair> completed_banks;
        std::shared_ptr<const JobSlotBankPair> completed_job_banks;
        std::shared_ptr<const SlotStreamBankPair> completed_stream_banks;
        std::shared_ptr<const StorageStreamAnalysisResult> completed_analysis;
        CheckpointJobFailure completed_job_failure;
        StorageState next=StorageState::completed;SlotDurability durability=record->durability;
        const char* code="";std::optional<SlotMediaError> completed_media_error;int system_error=0;
        try{
            ScopedCheckpointMemory scope(record->worker_memory);
            if(job.kind==JobKind::cleanup||(!record->cancellation.publication_started()&&record->cancellation.cancelled()))throw SlotMediaError("StorageCancelled");
            if(!media){
                if(startup_error_)throw *startup_error_;
                throw SlotMediaError(startup_message()[0]?startup_message():"StorageOpenFailed",startup_system_error_);
            }
            if(job.kind==JobKind::inspect){
                if(record->job){struct OwnedBanks{CheckpointMemoryCharge memory;std::shared_ptr<void> limit;SlotBankPair value;};auto banks=checkpoint_make_shared<OwnedBanks>();banks->limit=record->allocation;banks->value=media->inspect(record->slot,record->cancellation,&banks->memory);completed_banks=std::shared_ptr<const SlotBankPair>(banks,&banks->value);auto marker=record->job->reserve(CheckpointJobUse::candidate,0);if(!marker.ok)throw checkpoint_job_detail::FailureException{std::move(marker.error)};completed_job_banks=checkpoint_make_shared<JobSlotBankPair>(JobSlotBankPair::Key{},completed_banks,std::move(marker.value),checkpoint_shared_allocation_owner(banks),checkpoint_shared_allocation_owner(record->allocation),checkpoint_shared_allocation_owner(budget_));}
                else{auto banks=media->inspect(record->slot,record->cancellation);auto lease=record->allocation;completed_banks=std::shared_ptr<const SlotBankPair>(new SlotBankPair(std::move(banks)),[lease](const SlotBankPair* value){delete value;});}
                if(record->cancellation.cancelled())throw SlotMediaError("StorageCancelled");
            }else if(job.kind==JobKind::inspect_stream){completed_stream_banks=media->inspect_stream(record->slot,*record->job,record->cancellation);}
            else if(job.kind==JobKind::stage_stream){
                if(record->next_envelope){struct OwnedRequest{CheckpointMemoryCharge memory;SlotStreamWriteRequest value;};auto owned=checkpoint_make_shared<OwnedRequest>();owned->memory=CheckpointMemoryCharge(0);auto request=std::shared_ptr<SlotStreamWriteRequest>(owned,&owned->value);request->slot=SlotKey{StableId(record->slot.id),record->slot.kind,StableId(record->slot.profile_namespace_binding)};request->source=record->reference.source;request->expected=record->stream_banks;request->expected_generation=record->next_expected;request->new_generation=record->next_expected+1;request->inactive=record->next_inactive;request->envelope=std::move(record->next_envelope);request->job=record->job;record->stream=std::move(request);{std::lock_guard<std::mutex> lock(mutex_);record->analysis_result.reset();}}
                record->prepared=media->stage_stream(record->stream,record->cancellation);next=StorageState::awaiting_owner_validation;
            }
            else if(job.kind==JobKind::analyze_stream){
                if(!record->analysis)throw SlotMediaError("StorageInvalidAnalysis");
                auto result=record->analysis->analyze(record->slot,record->stream_banks.get(),record->prepared?record->prepared->stream_readback().get():nullptr,*record->job,record->cancellation);
                if(!result.ok)throw checkpoint_job_detail::FailureException{std::move(result.error)};
                // 私有解码完成后重核原两银行观察。此worker持唯一目录锁，coordinator在load终态前不排写。
                // fd/digest比较仍在原worker，不把读取或close搬到世界安装线程。
                if(!record->analysis_is_stage&&record->stream_banks)media->compare_stream_banks(record->slot,*record->stream_banks,*record->job,&record->cancellation);
                if(record->cancellation.cancelled())throw SlotMediaError("StorageCancelled");
                completed_analysis=std::move(result.value);if(!completed_analysis)throw SlotMediaError("StorageInvalidAnalysisResult");
                next=record->analysis_is_stage?StorageState::awaiting_owner_validation:StorageState::completed;
            }
            else if(job.kind==JobKind::analyze_import){
                if(!record->analysis||!record->job||record->import_path.empty())throw SlotMediaError("StorageInvalidImportStage");auto source=media->stage_import_file(record->import_path,record->import_sha256,engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile(),*record->job,record->cancellation);
                auto result=record->analysis->analyze(record->slot,nullptr,source.get(),*record->job,record->cancellation);if(!result.ok)throw checkpoint_job_detail::FailureException{std::move(result.error)};
                auto current=media->open_import_file(record->import_path,engineering_slot_capacity_profile().max_slot_bytes,*record->job,record->cancellation);if(current->size()!=source->size()||current->full_sha256()!=record->import_sha256)throw SlotMediaError("ImportSourceChanged");completed_analysis=std::move(result.value);if(!completed_analysis)throw SlotMediaError("StorageInvalidAnalysisResult");
            }
            else if(job.kind==JobKind::stage){record->prepared=media->stage_frozen(record->write,record->cancellation);next=StorageState::awaiting_owner_validation;}
            else if(job.kind==JobKind::promote){durability=media->promote_after_owner_validation(*record->prepared,record->cancellation);}
            else if(job.kind==JobKind::recover){durability=media->recover_same_bytes(*record->prepared);}
        }catch(checkpoint_job_detail::FailureException& error){
            const bool cancelled=error.error.code()==CheckpointJobFailureCode::cancelled;completed_job_failure=std::move(error.error);code=cancelled?"StorageCancelled":"CheckpointJobFailed";durability=record->prepared&&record->prepared->is_published()?SlotDurability::visible_unconfirmed:SlotDurability::not_published;next=durability==SlotDurability::visible_unconfirmed?StorageState::awaiting_recovery:cancelled?StorageState::cancelled:StorageState::failed;
        }catch(const SlotMediaError& error){
            completed_media_error.emplace(error);system_error=error.system_error();durability=error.durability();
            if(record->prepared&&record->prepared->is_published())durability=SlotDurability::visible_unconfirmed;
            next=durability==SlotDurability::visible_unconfirmed?StorageState::awaiting_recovery:std::string_view(error.what())=="StorageCancelled"?StorageState::cancelled:StorageState::failed;
        }catch(const std::bad_alloc&){if(record->job)completed_job_failure=record->job->capture_fixed_failure(CheckpointJobFailureCode::allocation_failed);code="StorageResourceLimit";durability=record->prepared&&record->prepared->is_published()?SlotDurability::visible_unconfirmed:SlotDurability::not_published;next=durability==SlotDurability::visible_unconfirmed?StorageState::awaiting_recovery:StorageState::failed;}
        catch(...){code="StorageInternalFailure";durability=record->prepared&&record->prepared->is_published()?SlotDurability::visible_unconfirmed:SlotDurability::not_published;next=durability==SlotDurability::visible_unconfirmed?StorageState::awaiting_recovery:StorageState::failed;}
        std::uint64_t stage_token=0;
        if(next==StorageState::awaiting_owner_validation&&job.kind!=JobKind::analyze_stream){
            if(next_stage_>=static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())){next=StorageState::failed;completed_media_error.reset();code="StorageOperationLimit";}
            else stage_token=++next_stage_;
        }
        if(terminal(next)&&record->prepared){
            const auto cleanup_error=record->prepared->cleanup();
            if(cleanup_error){next=StorageState::failed;completed_media_error.reset();code="StorageCleanupFailed";system_error=cleanup_error;}
            record->prepared.reset();
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 只读完成也与cancel在同一锁上排序；已受理取消不能在最后竞态里冒成读取成功。
            if((job.kind==JobKind::inspect||job.kind==JobKind::inspect_stream||job.kind==JobKind::analyze_stream||job.kind==JobKind::analyze_import)&&next==StorageState::completed&&record->cancellation.cancelled()){next=StorageState::cancelled;completed_media_error.reset();code="StorageCancelled";}
            if(record->job_failure.code()==CheckpointJobFailureCode::none&&completed_job_failure.code()!=CheckpointJobFailureCode::none)record->job_failure=std::move(completed_job_failure);
            if(stage_token!=0)record->stage_token=stage_token;
            if(next==StorageState::completed&&completed_banks){record->banks=std::move(completed_banks);record->job_banks=std::move(completed_job_banks);}
            if(next==StorageState::completed&&completed_stream_banks)record->stream_banks=std::move(completed_stream_banks);
            if((next==StorageState::completed||next==StorageState::awaiting_owner_validation)&&completed_analysis)record->analysis_result=std::move(completed_analysis);
            if(job.kind==JobKind::analyze_stream||job.kind==JobKind::analyze_import)record->analysis.reset();
            // All diagnostic transfer below is non-allocating. No string is formed after catching
            // bad_alloc/FailureException, and what() is only borrowed from the retained exception.
            record->media_error=std::move(completed_media_error);record->literal_code=code;
            record->state=next;record->durability=durability;record->system_error=system_error;
        }
    }
    // 已接受任务的终态保留为深值；只有句柄、未提升tmp与目录lease在ack前由worker释放。
    std::uint64_t last=0;
    for(;;){
        std::shared_ptr<Record> remaining;{
            std::lock_guard<std::mutex> lock(mutex_);const auto next=records_.upper_bound(last);if(next==records_.end())break;last=next->first;remaining=next->second;
        }
        remaining->prepared.reset();remaining->stream.reset();remaining->stream_banks.reset();remaining->next_envelope.reset();remaining->analysis.reset();
        // 关闭已移交未知结果，不能继续标记本实例仍可恢复；耐久值及原错误保持不变。
        {std::lock_guard<std::mutex> lock(mutex_);if(remaining->state==StorageState::awaiting_recovery)remaining->state=StorageState::closed_unconfirmed;}
    }
    media.reset();
    {std::lock_guard<std::mutex> lock(mutex_);closed_=true;}
}
} // 世界提交、完整槽语义和profile交付均不是本服务的成功含义。
