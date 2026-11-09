// 单次 capture/continuation 的拥有、取消与首错上下文；不授予世界、准入或发布权限。
#pragma once
#include "checkpoint_stream.hpp"
#include <memory>
#include <vector>
#include <type_traits>
#include <string_view>
#include <initializer_list>
namespace hanmo::resident {
struct CapacityProfile;
struct SlotCapacityProfile;
struct CheckpointJobBudgetState;
class CheckpointRetentionCensus;
class CheckpointJobBudget;
enum class CheckpointJobFailureCode:std::uint8_t {
    none,invalid_job,invalid_profile,control_budget,allocation_failed,cancelled,
    profile_mismatch,invalid_use,size_overflow,reservation_mismatch,write_range,
    capacity_exceeded,already_failed,diagnostic_capacity,legacy_error,json_context_required,default_shape_required,default_shape_toolchain
};
// 静态字面量；宿主fallback不复制映射，也不为取得code构造动态RuleError。
const char* checkpoint_job_failure_code_name(CheckpointJobFailureCode)noexcept;
// 无state时只有固定标量；有效job的第一失败持state，使诊断控制区长于façade。
class CheckpointJobFailure {
public:
    CheckpointJobFailure() noexcept=default;
    CheckpointJobFailure(const CheckpointJobFailure&)=delete;
    CheckpointJobFailure& operator=(const CheckpointJobFailure&)=delete;
    CheckpointJobFailure(CheckpointJobFailure&&) noexcept=default;
    CheckpointJobFailure& operator=(CheckpointJobFailure&&) noexcept=default;
    CheckpointJobFailureCode code() const noexcept;
    std::uint64_t required() const noexcept;
    // 只读借用，仅在此failure/lease仍活时消费；不提供移出裸RuleError的接口。
    const RuleError* rule_error() const noexcept;
    // 仅有效state能在已预留首错窗口里尝试一次；无state/失败后保持固定描述。
    bool try_materialize_rule_error() noexcept;
    static CheckpointJobFailure fixed(CheckpointJobFailureCode,std::uint64_t required=0) noexcept;
private:
    friend class CheckpointJobBudget;
    friend class CheckpointJobReservation;
    friend class CheckpointJobBytes;
    friend class CheckpointJobFailureAccess;
    friend class CheckpointJobFailureLease;
    std::shared_ptr<CheckpointJobBudgetState> state_;
    CheckpointJobFailureCode fixed_=CheckpointJobFailureCode::none;
    std::uint64_t required_=0;
};
// 回执可复制的只读拥有视图；共享同一首错state，无新控制块、裸RuleError或环形拥有。
class CheckpointJobFailureLease {
public:
    CheckpointJobFailureLease()noexcept=default;
    CheckpointJobFailureLease(CheckpointJobFailure&&)noexcept;
    CheckpointJobFailureLease(std::shared_ptr<const CheckpointJobFailure>)noexcept;
    CheckpointJobFailureLease(const CheckpointJobFailureLease&)noexcept;
    CheckpointJobFailureLease& operator=(const CheckpointJobFailureLease&)noexcept;
    CheckpointJobFailureLease(CheckpointJobFailureLease&&)noexcept=default;
    CheckpointJobFailureLease& operator=(CheckpointJobFailureLease&&)noexcept=default;
    CheckpointJobFailureLease& operator=(CheckpointJobFailure&&)noexcept;
    CheckpointJobFailureLease& operator=(std::shared_ptr<const CheckpointJobFailure>)noexcept;
    static CheckpointJobFailureLease retain(const CheckpointJobFailure&)noexcept;
    bool try_materialize_rule_error()const noexcept;
    const CheckpointJobFailure* get()const noexcept{return alias_?alias_.get():(value_.code()!=CheckpointJobFailureCode::none?&value_:nullptr);}
    const CheckpointJobFailure* operator->()const noexcept{return get();}
    explicit operator bool()const noexcept{return get()!=nullptr;}
    void reset()noexcept;
private:
    CheckpointJobFailure value_;
    std::shared_ptr<const CheckpointJobFailure> alias_;
};
template<class T> struct CheckpointJobResult {
    bool ok=false;
    T value{};
    CheckpointJobFailure error{};
    CheckpointJobResult() noexcept(std::is_nothrow_default_constructible<T>::value)=default;
    CheckpointJobResult(bool success,T&& result,CheckpointJobFailure&& failure) noexcept(std::is_nothrow_move_constructible<T>::value)
        :ok(success),value(std::move(result)),error(std::move(failure)){}
    CheckpointJobResult(const CheckpointJobResult&)=delete;
    CheckpointJobResult& operator=(const CheckpointJobResult&)=delete;
    CheckpointJobResult(CheckpointJobResult&&) noexcept(std::is_nothrow_move_constructible<T>::value)=default;
    CheckpointJobResult& operator=(CheckpointJobResult&&) noexcept(std::is_nothrow_move_assignable<T>::value)=default;
};
using CheckpointJobStatus=CheckpointJobResult<Done>;
namespace checkpoint_job_detail { struct FailureException { CheckpointJobFailure error; }; }
// 每份分配在total仅记一次；位可同时约束原encode/scratch/candidate三个子上限。
enum class CheckpointJobUse:std::uint8_t { slot=0,encode=1,scratch=2,candidate=4,candidate_scratch=6 };
struct CheckpointJobUsage {
    std::uint64_t total_live=0,total_peak=0,encode_live=0,encode_peak=0;
    std::uint64_t scratch_live=0,scratch_peak=0,candidate_live=0,candidate_peak=0;
    bool cancelled=false;
};
// move-only token拥有内部账本，允许比job facade活得久；析构无分配且只退一次。
class CheckpointJobReservation {
public:
    CheckpointJobReservation() noexcept=default;
    ~CheckpointJobReservation() noexcept;
    CheckpointJobReservation(const CheckpointJobReservation&)=delete;
    CheckpointJobReservation& operator=(const CheckpointJobReservation&)=delete;
    CheckpointJobReservation(CheckpointJobReservation&&) noexcept;
    CheckpointJobReservation& operator=(CheckpointJobReservation&&) noexcept;
    std::uint64_t bytes()const noexcept{return bytes_;}
    bool valid()const noexcept{return bool(state_);}
    bool belongs_to(const CheckpointJobBudget&)const noexcept;
    // 固定随原 job 的瞬态工程模式；token 的真实容量和归属不随关账消失。
    bool memory_accounting_enabled()const noexcept;
    void retain_control(CheckpointRetentionCensus&)const;
    // 借现有state保第一失败；不制造失败、不分配新的诊断或控制块。
    CheckpointJobFailureLease failure_lease()const noexcept;
    // 技术首错与原reservation拥有同state；不复制诊断或新建控制块。
    CheckpointJobFailure capture_fixed_failure(CheckpointJobFailureCode,std::uint64_t required=0)const noexcept;
    CheckpointJobFailure capture_boundary_failure(std::string_view,std::initializer_list<std::string_view>,ErrorKind)const noexcept;
    CheckpointJobFailure capture_capacity_failure(std::initializer_list<std::string_view> resource,std::uint64_t required,std::string_view profile,std::uint64_t revision)const noexcept;
    bool shares_job(const CheckpointJobReservation& other)const noexcept{return state_&&state_==other.state_;}
    // grow必须在真实分配前；额外子约束不再加total，已取消job不接新预留。
    CheckpointJobStatus grow(std::uint64_t);
    // Sibling lifetime, original ledger and reserve implementation; never a new job.
    CheckpointJobResult<CheckpointJobReservation> reserve_related(CheckpointJobUse,std::uint64_t)const;
    CheckpointJobFailure capture_failure(RuleError&&)const noexcept;
    // 只有对应真实分配已销毁才可退还；无分配，超额请求不改变账。
    bool release_destroyed(std::uint64_t) noexcept;
    CheckpointJobStatus add_constraints(CheckpointJobUse)const;
    // 合并只转移同job/同use的已有收费，不创建第二份分配账。
    CheckpointJobStatus merge(CheckpointJobReservation&&);
private:
    friend class CheckpointJobBudget;
    friend class CheckpointJobBytes;
    std::shared_ptr<CheckpointJobBudgetState> state_;
    std::uint64_t bytes_=0;
    mutable std::uint8_t use_=0;
    CheckpointJobReservation(std::shared_ptr<CheckpointJobBudgetState>,std::uint64_t,std::uint8_t) noexcept;
    void reset() noexcept;
};
// 工程配置在入口核完整13/8字段指纹；仅深拥有受限ID/固定标量，不复制map或借栈profile。
class CheckpointJobBudget {
public:
    CheckpointJobBudget() noexcept=default;
    CheckpointJobBudget(const CheckpointJobBudget&)=delete;
    CheckpointJobBudget& operator=(const CheckpointJobBudget&)=delete;
    CheckpointJobBudget(CheckpointJobBudget&&) noexcept=default;
    CheckpointJobBudget& operator=(CheckpointJobBudget&&) noexcept=default;
    // 生产唯一默认入口关闭严格内存累计；粗档大小及业务验证仍由原调用者执行。
    static CheckpointJobResult<CheckpointJobBudget> create(const CapacityProfile&,const SlotCapacityProfile&);
    // 仅内部旧计量单测使用；不从作者、存档或公开配置选择。
    static CheckpointJobResult<CheckpointJobBudget> create_metered_for_verification(const CapacityProfile&,const SlotCapacityProfile&);
    bool memory_accounting_enabled()const noexcept;
    CheckpointJobStatus check_profile(const CapacityProfile&)const;
    CheckpointJobStatus check_profile(const SlotCapacityProfile&)const;
    CheckpointJobResult<CheckpointJobReservation> reserve(CheckpointJobUse,std::uint64_t);
    CheckpointJobUsage usage()const noexcept;
    void cancel() noexcept;
    // 仅显式运输边界使用：移动第一份legacy错误入state，失败租约保持到回传消费者完成。
    CheckpointJobFailure capture_failure(RuleError&&) noexcept;
    // Original boundary diagnostics are constructed once inside the reserved first-failure window.
    CheckpointJobFailure capture_boundary_failure(std::string_view code,std::initializer_list<std::string_view> path,ErrorKind) noexcept;
    CheckpointJobFailure capture_capacity_failure(std::initializer_list<std::string_view> resource,std::uint64_t required,std::string_view profile,std::uint64_t revision)noexcept;
    CheckpointJobFailure capture_fixed_failure(CheckpointJobFailureCode,std::uint64_t required=0) noexcept;
private:
    friend class CheckpointJobReservation;
    std::shared_ptr<CheckpointJobBudgetState> state_;
};
// 大字节和预留同拥有对象移动；销毁先释放真实Bytes再退账，不暴露可resize的vector。
class CheckpointJobBytes {
public:
    using Buffer=std::vector<std::uint8_t>;
    CheckpointJobBytes() noexcept=default;
    CheckpointJobBytes(const CheckpointJobBytes&)=delete;
    CheckpointJobBytes& operator=(const CheckpointJobBytes&)=delete;
    CheckpointJobBytes(CheckpointJobBytes&&) noexcept=default;
    CheckpointJobBytes& operator=(CheckpointJobBytes&&) noexcept;
    static CheckpointJobResult<CheckpointJobBytes> allocate(CheckpointJobBudget&,CheckpointJobUse,std::uint64_t);
    static CheckpointJobResult<CheckpointJobBytes> copy(CheckpointJobBudget&,CheckpointJobUse,const Buffer&);
    // 只接已有预留后的实际分配，核capacity；本函数本身不声称原producer已语义合法。
    static CheckpointJobResult<CheckpointJobBytes> adopt_reserved(Buffer&&,CheckpointJobReservation&&);
    const Buffer& bytes()const noexcept{return bytes_;}
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return reservation_.belongs_to(job);}
    // Actual immutable Bytes capacity plus original owning token; no wire-size estimate.
    void retain(CheckpointRetentionCensus&)const;
    CheckpointJobStatus add_constraints(CheckpointJobUse use)const{return reservation_.add_constraints(use);}
    CheckpointJobStatus write(std::size_t offset,const std::uint8_t*,std::size_t);
private:
    CheckpointJobReservation reservation_;
    Buffer bytes_;
};
} // 账本只核已接入的真实分配，未接world clone/HMCN2内层不得借本类型宣称全job闭合。
