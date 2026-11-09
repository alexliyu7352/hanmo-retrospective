// 原world线程的真实保存消费者；存储worker只收冻结字节，不能持session或执行世界核验。
#pragma once
#include "resident_slot_codec.hpp"
#include "resident_storage_service.hpp"
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/world_restore_candidate.hpp"

namespace hanmo::resident::storage {
class ResidentProfileCoordinator;
class StoredSlotReceipt;
enum class SlotSaveState { inspecting,awaiting_preflight,staging,awaiting_validation,promoting,awaiting_recovery,cancelling,completed,cancelled,failed,closed_unconfirmed };
// 有限回执不公开原payload或可绕过核心的提升token，source始终是实际捕获时的身份。
struct SlotSavePoll {
    bool ok=false;StorageDiagnostic code;StorageOperationRef reference;
    SlotSaveState state=SlotSaveState::inspecting;SlotDurability durability=SlotDurability::not_published;
    Integer generation=0;int system_error=0;StorageCoreError core_error;
    CheckpointJobFailureLease job_failure;std::shared_ptr<const StoredSlotReceipt> stored;
};
#ifdef HANMO_STORAGE_TEST_FAULTS
// 仅测试在已经接受票据及首次poll处注入单次分配失败，核故障时原票据仍能终结。
struct SlotCoordinatorTestHooks { std::function<void()> after_inspect_accepted,before_poll; };
#endif
enum class SlotLoadState { inspecting,validating,awaiting_install,installed,cancelled,failed };
struct SlotLoadPoll {bool ok=false;StorageOperationRef reference;SlotLoadState state=SlotLoadState::inspecting;Integer generation=0;WorldIdentity installed_identity;bool profile_sync_blocked=true;StorageDiagnostic code;CheckpointJobFailureLease failure;};
// 构造与begin/advance/recover/release必须在session所属world线程；poll/cancel/close只接触运输状态。
class ResidentSlotCoordinator {
public:
    ResidentSlotCoordinator(std::string directory,StorageServiceLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
        ,SlotIoTestHooks hooks={},SlotCoordinatorTestHooks coordinator_hooks={}
#endif
    );
    ~ResidentSlotCoordinator();
    ResidentSlotCoordinator(const ResidentSlotCoordinator&)=delete;
    ResidentSlotCoordinator& operator=(const ResidentSlotCoordinator&)=delete;
    // 公开请求仅接受slot与期望代数，source/载荷/continuation全部在此调用真实core捕获。
    StorageAccepted begin_save(ResidentReplaySession&,const std::string& slot,Integer expected_generation);
    // 完整槽/2同真实core捕获和同epoch胶囊；现实时间仅在此入口冻结一次。
    StorageAccepted begin_save_v2(ResidentReplaySession&,ResidentProfileCoordinator&,const std::string& slot,Integer expected_generation);
    // 完整槽/2读路径：原目录工厂、原worker有界解码、同session一次安装。工程profile上限显式固定。
    StorageAccepted begin_load(ResidentReplaySession&,const BoundaryValue& catalog,const StableId& scenario,const CatalogIdentity& expected_catalog,const std::string& slot);
    StorageStatus advance_load(ResidentReplaySession&,ResidentProfileCoordinator&);
    SlotLoadPoll poll_load(const StorageOperationRef&)const;
    StorageStatus cancel_load(const StorageOperationRef&);
    StorageStatus release_load(const StorageOperationRef&);
    // 每次推进一个明确阶段；实际核验只发生在此所属world线程，不藏在poll里。
    StorageStatus advance(ResidentReplaySession&);
    // 跨线程poll仅复制有限缓存，不做文件I/O、JSON解码、核验或session调用。
    SlotSavePoll poll(const StorageOperationRef&)const;
    // 取消与close可从运输线程请求；发布开始后的实际结果不能被改成撤销。
    StorageStatus cancel(const StorageOperationRef&);
    void request_close();
    bool close_complete()const;
    // 补同字节屏障仍消费原物理阶段，不生成新代或重投世界。
    StorageStatus recover_same_bytes(const StorageOperationRef&);
    // 只有world线程可回收终态及大字节，外部旧票据不会绑定到下一次保存。
    StorageStatus release(const StorageOperationRef&);
private:
    struct Pending;struct PendingLoad;struct PendingStream;
    std::shared_ptr<PendingStream> pending_stream_;
    StorageStatus advance_stream_save(ResidentReplaySession&);
    StorageStatus recover_stream_save(const StorageOperationRef&);
    StorageStatus release_stream_save(const StorageOperationRef&);
    void publish_stream(SlotSaveState,const StoragePoll* =nullptr);
    std::shared_ptr<PendingLoad> pending_load_;
    std::unique_ptr<Pending> pending_;
    ResidentStorageService storage_;
    const std::thread::id owner_;
    std::atomic<bool> cancelling_{false},closing_{false};
    mutable std::mutex mutex_;SlotSavePoll cached_;SlotLoadPoll load_cached_;
    std::shared_ptr<WorldInstallCancellation> load_cancel_;std::optional<StorageOperationRef> active_;
#ifdef HANMO_STORAGE_TEST_FAULTS
    SlotCoordinatorTestHooks test_hooks_;
#endif
    bool owner_thread()const noexcept;
    void publish(SlotSaveState,StorageDiagnostic code={},SlotDurability=SlotDurability::not_published,int system_error=0,StorageCoreError error={});
    void finish(const StoragePoll&);
    void fail_before_stage(StorageDiagnostic,StorageCoreError error={});
    void reject_staged(StorageDiagnostic,StorageCoreError error={});
    bool validated(ResidentReplaySession&,const Bytes&,const std::optional<Bytes>&,const ResidentSlotSelected&);
};
static_assert(std::is_nothrow_copy_constructible_v<SlotSavePoll>,"storage poll must not allocate another diagnostic");
static_assert(std::is_nothrow_copy_constructible_v<SlotLoadPoll>,"storage poll must not allocate another diagnostic");
} // 单个未释放保存是明确有界队列；原GD HM-SAVE继续走原路径。
