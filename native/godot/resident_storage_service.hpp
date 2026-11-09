// 有界槽请求服务只在专用C++线程执行介质；主线程轮询深拥有结果，不等待磁盘。
#pragma once
#include "resident_slot_media.hpp"
#include "resident_storage_diagnostic.hpp"
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

namespace hanmo::resident::storage {
namespace storage_service_detail {
// The service index spans jobs. A node binds the current record resource at its actual
// allocation, then carries that resource in the existing allocation header until deallocation.
// No global allocator silently binds every later operation to the service-construction scope.
template<class T>struct RecordIndexAllocator {
    using value_type=T;using is_always_equal=std::true_type;
    RecordIndexAllocator()noexcept=default;
    template<class U>RecordIndexAllocator(const RecordIndexAllocator<U>&)noexcept{}
    T* allocate(std::size_t count){if(count>SIZE_MAX/sizeof(T))throw std::bad_alloc();return static_cast<T*>(checkpoint_allocate_object(count*sizeof(T),alignof(T)));}
    void deallocate(T* value,std::size_t)noexcept{checkpoint_deallocate_object(value);}
    template<class U>bool operator==(const RecordIndexAllocator<U>&)const noexcept{return true;}
    template<class U>bool operator!=(const RecordIndexAllocator<U>&)const noexcept{return false;}
};
}
class ProfileValidatedStage;
class StorageStreamAnalysisResult {public:virtual ~StorageStreamAnalysisResult()=default;};
// 只由封闭slot coordinator排入原worker；实现拥有冻结目录/准入输入，不得借active world。
class StorageStreamAnalysis {
public:
    virtual ~StorageStreamAnalysis()=default;
    virtual CheckpointJobResult<std::shared_ptr<const StorageStreamAnalysisResult>> analyze(
        const SlotKey&,const SlotStreamBankPair*,SlotReadLease*,CheckpointJobBudget&,const SlotCancellation&)const=0;
};
// 运输实例/操作/世界来源各自绑定，旧世界票据仍能领取自身实际磁盘终态。
struct StorageOperationRef { std::uint64_t instance=0,operation=0; WorldIdentity source; };
// 接纳前首错直接随move-only结果返回；预算已满时不再申请shared控制块。
struct StorageAccepted {
    bool ok=false;StorageOperationRef reference;StorageDiagnostic code;
    CheckpointJobFailureLease job_failure;
    CheckpointJobFailureCode fixed_failure=CheckpointJobFailureCode::none;std::uint64_t required=0;
    CheckpointJobFailure owned_failure;
    static StorageAccepted rejected(CheckpointJobFailure&& error)noexcept{StorageAccepted result;result.fixed_failure=error.code();result.required=error.required();result.owned_failure=std::move(error);return result;}
    CheckpointJobFailureLease take_failure()noexcept{if(owned_failure.code()!=CheckpointJobFailureCode::none)return CheckpointJobFailureLease(std::move(owned_failure));return std::move(job_failure);}
    const CheckpointJobFailure* failure()const noexcept{return owned_failure.code()!=CheckpointJobFailureCode::none?&owned_failure:job_failure.get();}
};
struct StorageStatus { bool ok=false; StorageDiagnostic code; };
enum class StorageState { queued,running,awaiting_owner_validation,awaiting_recovery,completed,cancelled,failed,closed_unconfirmed };
// awaiting_owner_validation时frozen_write.bytes已与实际tmp逐字节读回相同；它仍不是语义通过。
struct StoragePoll {
    bool ok=false; StorageDiagnostic code; StorageState state=StorageState::queued;
    SlotDurability durability=SlotDurability::not_published;
    int system_error=0; std::uint64_t stage_token=0;
    std::shared_ptr<const SlotWriteRequest> frozen_write;
    std::shared_ptr<const SlotBankPair> banks;
    std::shared_ptr<const JobSlotBankPair> job_banks;
    CheckpointJobFailureLease job_failure;
    std::shared_ptr<const StorageStreamAnalysisResult> stream_analysis;
};
struct StorageServiceLimits { SlotMediaLimits media; std::size_t max_operations=0,max_owned_bytes=0; };
// promote只接受本实例原票据和原读回阶段token；调用者必须先走原codec/core核验，不能传bool放行。
class ResidentStorageService {
public:
    ResidentStorageService(std::string directory,StorageServiceLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
        ,SlotIoTestHooks hooks={}
#endif
    );
    ~ResidentStorageService();
    ResidentStorageService(const ResidentStorageService&)=delete;
    ResidentStorageService& operator=(const ResidentStorageService&)=delete;
    StorageAccepted inspect(const SlotKey&,const WorldIdentity&);
    StorageAccepted stage(const SlotWriteRequest&);
    StorageAccepted inspect_stream(const SlotKey&,const WorldIdentity&,std::shared_ptr<CheckpointJobBudget>);
    StoragePoll poll(const StorageOperationRef&) const;
    // 只消费profile coordinator封闭能力；此入口永远不能提升world槽。
    StorageStatus promote_profile_metadata(const StorageOperationRef&,ProfileValidatedStage&&);
#ifndef HANMO_STORAGE_TEST_FAULTS
private:
    friend class ResidentSlotCoordinator;
    friend class ResidentSlotStreamCoordinator;
    friend class ResidentProfileCoordinator;
#endif
    // 生产只允许真正调用原core核验的coordinator提升；测试宏仅为独立介质故障门开放。
    // 接续同票据的bank/stage实际解析和私有candidate核验，所有pread仍在原worker。
    StorageAccepted stage_stream(const SlotStreamWriteRequest&);
    StorageAccepted inspect_profile_metadata(const SlotKey&,const WorldIdentity&,std::shared_ptr<CheckpointJobBudget>);
    StorageAccepted stage_profile_metadata(const SlotWriteRequest&,std::shared_ptr<CheckpointJobBudget>);
    // 原profile创建的封闭快照证明也在此worker执行；此口不铸造任何落盘成功。
    StorageAccepted analyze_import_file(const std::string& path,const CheckpointDigest&,const WorldIdentity&,std::shared_ptr<CheckpointJobBudget>,std::shared_ptr<const StorageStreamAnalysis>);
    StorageStatus revalidate_import_file(const StorageOperationRef&,std::shared_ptr<const StorageStreamAnalysis>);
    StorageAccepted analyze_profile_capture(const SlotKey&,const WorldIdentity&,std::shared_ptr<CheckpointJobBudget>,std::shared_ptr<const StorageStreamAnalysis>);
    // 继续原inspect票据；真实银行lease只留worker，不经poll泄到主线程关闭fd。
    StorageStatus continue_stream_stage(const StorageOperationRef&,Integer expected_generation,SlotBank inactive,std::shared_ptr<const FrozenSlotEnvelope>);
    StorageStatus analyze_stream(const StorageOperationRef&,std::uint64_t stage_token,std::shared_ptr<const StorageStreamAnalysis>);
    StorageStatus promote_after_owner_validation(const StorageOperationRef&,std::uint64_t stage_token);
#ifndef HANMO_STORAGE_TEST_FAULTS
public:
#endif
    StorageStatus recover_same_bytes(const StorageOperationRef&,std::uint64_t stage_token);
    StorageStatus cancel(const StorageOperationRef&);
    StorageStatus release(const StorageOperationRef&);
    void request_close();
    bool close_complete() const;
private:
    enum class JobKind { inspect,stage,inspect_stream,stage_stream,analyze_stream,analyze_import,promote,recover,cleanup,dispose };
    struct Record;
    struct Budget;
    struct Job { std::uint64_t id; JobKind kind; };
    std::string directory_; StorageServiceLimits limits_; std::uint64_t instance_=0,next_operation_=0,next_stage_=0;
    std::shared_ptr<Budget> budget_;
    mutable std::mutex mutex_; std::condition_variable wake_;
    using RecordIndexValue=std::pair<const std::uint64_t,std::shared_ptr<Record>>;
    std::map<std::uint64_t,std::shared_ptr<Record>,std::less<std::uint64_t>,storage_service_detail::RecordIndexAllocator<RecordIndexValue>> records_;
    // One queued phase per record. Its FIFO link is part of the already-owned record, so
    // cancellation/recovery/disposal never needs another allocation after a job has failed.
    Record* queue_head_=nullptr;Record* queue_tail_=nullptr;
    // Worker catches retain the already-constructed media exception without allocating a second diagnostic.
    // Fixed fallback literals are copied inline; poll/accept retain the original media diagnostic without allocation.
    bool closing_=false,closed_=false; std::optional<SlotMediaError> startup_error_;
    const char* startup_code_="";int startup_system_error_=0;
    const char* startup_message()const noexcept{return startup_error_?startup_error_->what():startup_code_;}
    std::thread worker_;
#ifdef HANMO_STORAGE_TEST_FAULTS
    SlotIoTestHooks hooks_;
#endif
    std::shared_ptr<Record> find(const StorageOperationRef&) const;
    StorageAccepted accept(std::shared_ptr<Record>,JobKind);
    bool enqueue(Record&,JobKind)noexcept;
    std::shared_ptr<Record> dequeue(Job&)noexcept;
    void work();
};
static_assert(std::is_nothrow_copy_constructible_v<StorageStatus>,"storage poll must not allocate another diagnostic");
static_assert(std::is_nothrow_copy_constructible_v<StoragePoll>,"storage poll must not allocate another diagnostic");
} // 析构仅为最后安全兜底可join；宿主先request_close并轮询ack，再释放服务。
