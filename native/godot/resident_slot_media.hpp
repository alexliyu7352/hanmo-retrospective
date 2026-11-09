// 槽介质只办理既有封套的精确字节；不解码世界、不授予准入、不形成世界或StoredSlotReceipt。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include <atomic>
#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace hanmo::resident::storage {
// 原两种世界槽与封闭本机元档案target显式区分；不能借格式号猜测或迁移。
enum class SlotFileKind { legacy_save, resident_checkpoint, profile_metadata, slot_envelope_v2 };
enum class SlotBank { a, b };
enum class SlotDurability { not_published, visible_unconfirmed, durable_confirmed };
// 目录为受信配置；world保持原3auto/10manual，profile仅固定metadata名字及host绑定。
struct SlotKey { std::string id; SlotFileKind kind=SlotFileKind::resident_checkpoint;std::string profile_namespace_binding{}; };
// 字节是独立拥有的实际文件内容；mtime只供显示，不参与CAS或选代。
struct SlotBankImage { bool present=false; Bytes bytes; std::uint64_t modified_unix=0; };
struct SlotBankPair { SlotBankImage a,b; };
// 原worker实际读取的同job银行拥有值；构造钥匙不向任意Bytes调用者开放。
class ResidentStorageService;
class ResidentProfileCoordinator;
class JobSlotBankPair {
public:
    class Key{Key()=default;friend class ResidentStorageService;friend class ResidentProfileCoordinator;};
    JobSlotBankPair(Key,std::shared_ptr<const SlotBankPair> value,CheckpointJobReservation&& token,std::shared_ptr<CheckpointAllocationOwner> owner,std::shared_ptr<CheckpointAllocationOwner> limit_owner={},std::shared_ptr<CheckpointAllocationOwner> service_owner={}):token_(std::move(token)),owner_(std::move(owner)),limit_owner_(std::move(limit_owner)),service_owner_(std::move(service_owner)),value_(std::move(value)){}
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return value_&&token_.belongs_to(job);}
    const SlotBankPair& value()const noexcept{return *value_;}
    void retain(CheckpointRetentionCensus&)const;
private:CheckpointJobReservation token_;std::shared_ptr<CheckpointAllocationOwner> owner_,limit_owner_,service_owner_;std::shared_ptr<const SlotBankPair> value_;
};
// 代数和inactive选择由原codec作出；介质再核递增形状和两银行精确字节CAS。
struct SlotWriteRequest {
    SlotKey slot; SlotBank inactive=SlotBank::b; WorldIdentity source;
    Integer expected_generation=0,new_generation=1;
    SlotBankPair expected; Bytes bytes;
};
// 流银行保存真实文件lease与读取时完整摘要，不复制整档；固定fd读口核inode代际。
class SlotReadLease:public CheckpointReadSource {
public:
    virtual const CheckpointDigest& full_sha256()const noexcept=0;
    virtual std::uint64_t modified_unix()const noexcept=0;
};
struct SlotStreamBankPair { std::shared_ptr<SlotReadLease> a,b; };
class FrozenSlotEnvelope;
// job由本请求共享拥有，冻结producer与实际输出均留在同一预算生命周期。
struct SlotStreamWriteRequest {
    SlotKey slot;SlotBank inactive=SlotBank::b;WorldIdentity source;
    Integer expected_generation=0,new_generation=1;
    std::shared_ptr<const SlotStreamBankPair> expected;
    std::shared_ptr<const FrozenSlotEnvelope> envelope;
    std::shared_ptr<CheckpointJobBudget> job;
};
// 有限工程预算由调用者明确给出；既有格式上限仍由原codec实施，不由磁盘文件自升配额。
struct SlotMediaLimits {
    std::size_t max_file_bytes=0,max_read_pair_bytes=0;
    std::uint64_t max_staging_disk_bytes=0;
    // 仅受信host构造指定；非空时该worker只打开本机元档案的固定两银行，不允许world槽。
    std::string host_profile_namespace_binding{};
};
// 失败保准确阶段与系统码；提升后的任何未确认错误都不能伪装成未写入。
class SlotMediaError : public std::runtime_error {
public:
    SlotMediaError(std::string code,int system_error=0,SlotDurability durability=SlotDurability::not_published);
    int system_error() const noexcept { return system_error_; }
    SlotDurability durability() const noexcept { return durability_; }
private:
    friend class ResidentSlotMedia;
    SlotMediaError with_durability(SlotDurability)const noexcept;
    int system_error_; SlotDurability durability_;
};
// 最后取消与开始rename由单次CAS排序；进入publishing后取消只报告未能撤销，等待实际结果。
class SlotCancellation {
public:
    bool request() noexcept;
    bool cancelled() const noexcept;
    bool begin_publish() noexcept;
    void published() noexcept;
    bool publication_started() const noexcept;
private:
    enum class State { open,cancelled,publishing,published };
    std::atomic<State> state_{State::open};
};
// 测试只在明确编译开关下拦截具名系统调用阶段，不能作为生产验证成功入口。
enum class SlotIoPoint { before_inspect,before_write,file_sync,file_close,stage_readback,before_publish,after_rename,directory_sync,final_readback,recovery_file_sync,recovery_directory_sync };
#ifdef HANMO_STORAGE_TEST_FAULTS
struct SlotIoTestHooks { std::function<int(SlotIoPoint)> before; };
#endif
struct SlotDirectory;
// 阶段对象仅介质可构造/修改；原核验读回后只能提升这同一份冻结输入，不能传另一包替换。
class PreparedSlotWrite:public CheckpointAllocatedObject {
public:
    ~PreparedSlotWrite();
    PreparedSlotWrite(const PreparedSlotWrite&)=delete;
    PreparedSlotWrite& operator=(const PreparedSlotWrite&)=delete;
    PreparedSlotWrite(PreparedSlotWrite&&)=delete;
    PreparedSlotWrite& operator=(PreparedSlotWrite&&)=delete;
    const SlotWriteRequest& request() const noexcept { return *request_; }
    const std::shared_ptr<const SlotStreamWriteRequest>& stream_request()const noexcept{return stream_request_;}
    const std::shared_ptr<SlotReadLease>& stream_readback()const noexcept{return stream_readback_;}
    bool is_published() const noexcept { return published_; }
private:
    friend class ResidentSlotMedia;
    friend class ResidentStorageService;
    PreparedSlotWrite(std::shared_ptr<SlotDirectory>,std::shared_ptr<const SlotWriteRequest>,std::string,std::uint64_t);
    CheckpointJobReservation job_control_;
    CheckpointMemoryCharge owned_fields_;
    std::shared_ptr<SlotDirectory> directory_;
    std::shared_ptr<const SlotWriteRequest> request_;
    std::shared_ptr<const SlotStreamWriteRequest> stream_request_;
    std::shared_ptr<SlotReadLease> stream_readback_;
    std::string temporary_;
    std::uint64_t disk_charge_=0;
    bool published_=false,cleanup_attempted_=false;
    // 只在worker明确清理；失败保目录预留，调用者据errno报告而不伪称已删除。
    int cleanup() noexcept;
};
// 同步实现只由专用存储worker调用；原语义owner负责最高合法代选择与同核readback验证。
class ResidentSlotMedia {
public:
    ResidentSlotMedia(const std::string& directory,SlotMediaLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
        ,SlotIoTestHooks hooks={}
#endif
    );
    ~ResidentSlotMedia();
    ResidentSlotMedia(const ResidentSlotMedia&)=delete;
    ResidentSlotMedia& operator=(const ResidentSlotMedia&)=delete;
    SlotBankPair inspect(const SlotKey&,const SlotCancellation&,CheckpointMemoryCharge* owned_memory=nullptr);
    std::shared_ptr<const SlotStreamBankPair> inspect_stream(const SlotKey&,CheckpointJobBudget&,const SlotCancellation&);
    std::shared_ptr<PreparedSlotWrite> stage_stream(std::shared_ptr<const SlotStreamWriteRequest>,SlotCancellation&);
    std::shared_ptr<PreparedSlotWrite> stage(std::shared_ptr<const SlotWriteRequest>,SlotCancellation&);
    SlotDurability promote_after_owner_validation(PreparedSlotWrite&,SlotCancellation&);
    SlotDurability recover_same_bytes(PreparedSlotWrite&);
    static bool valid_slot(const std::string&) noexcept;
    // 世界槽仍沿原allowlist；profile target须固定名字和完整namespace摘要。
    static bool valid_key(const SlotKey&) noexcept;
private:
    friend class ResidentStorageService;
    // 只有服务已深复制并记账的输入可走此私有接头；其它调用必须用stage再冻结。
    std::shared_ptr<PreparedSlotWrite> stage_frozen(std::shared_ptr<const SlotWriteRequest>,SlotCancellation&);
    std::shared_ptr<SlotDirectory> directory_; SlotMediaLimits limits_;
#ifdef HANMO_STORAGE_TEST_FAULTS
    SlotIoTestHooks hooks_;
#endif
    std::uint64_t temporary_sequence_=0;
    void point(SlotIoPoint) const;
    void compare_banks(const SlotKey&,const SlotBankPair&,const SlotCancellation*) const;
    void verify_file(const char*,const Bytes&,const SlotCancellation*) const;
    void compare_bank(const char*,const SlotBankImage&,const SlotCancellation*) const;
    void verify_published(const SlotWriteRequest&) const;
    static std::shared_ptr<SlotReadLease> open_import_file(const std::string&,std::uint64_t trusted_limit,CheckpointJobBudget&,const SlotCancellation&);
    // 外部输入先复制为本job独占匿名文件；decoder永远不直接读取外部可变FD。
    std::shared_ptr<SlotReadLease> stage_import_file(const std::string&,const CheckpointDigest&,const CapacityProfile&,const SlotCapacityProfile&,CheckpointJobBudget&,const SlotCancellation&);
    std::shared_ptr<SlotReadLease> open_stream(const char*,CheckpointJobBudget&,const SlotCancellation*,bool absent=false)const;
    void compare_stream_banks(const SlotKey&,const SlotStreamBankPair&,CheckpointJobBudget&,const SlotCancellation*)const;
    void verify_stream_file(const char*,const FrozenSlotEnvelope&,CheckpointJobBudget&,const SlotCancellation*)const;
    SlotDurability promote_stream(PreparedSlotWrite&,SlotCancellation&);
    SlotDurability recover_stream(PreparedSlotWrite&);
    // Closed original names have a maximum of 28 characters including the
    // profile target. The full name stays on the call stack, never a heap concat.
    struct BankName { std::array<char,32> bytes{};const char* c_str()const noexcept{return bytes.data();} };
    BankName bank_name(const SlotKey&,SlotBank) const;
};
} // 只有原槽服务可用的内部介质接口，不注册为Godot公开保存方法。
