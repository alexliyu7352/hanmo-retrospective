// 容量只由编译或受信配置授予；本类型从不从检查点、MOD或GD输入解码。
#pragma once
#include "checkpoint_job_budget.hpp"
#include "hanmo/resident/model_contract.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
namespace hanmo::resident {
// 原生关账试点仍保整档粗上限；256 MB 明确按十进制字节，不是内存租约。
inline constexpr std::uint64_t checkpoint_coarse_file_limit_bytes = 256000000;
// 损坏长度必须在分配/循环前拒绝；先核游标，不能让无符号相减回绕。
constexpr bool
checkpoint_count_fits_remaining(std::uint64_t count, std::uint64_t total,
                                std::uint64_t consumed) noexcept {
  return consumed <= total && count <= total - consumed;
}
class CheckpointMemoryResource;
// 仅保真实文件／结构／磁盘安全界限；不登记逐型内存或集合额度。
struct CapacityProfile {
  StableId profile_id;
  std::uint64_t revision = 0;
  std::uint64_t max_checkpoint_bytes = 0;
  std::uint64_t max_header_bytes = 0;
  std::map<StableId, std::uint64_t> max_section_bytes_by_registered_name;
  std::uint64_t max_nesting_depth = 0;
  std::uint64_t max_bigint_encoded_bytes = 0;
  std::uint64_t stream_chunk_bytes = 0;
  std::uint64_t max_staging_disk_bytes = 0;
};
// SlotCapacityProfile/1独立约束封套和续行；不能用checkpoint预算替续行自行扩容。
struct SlotCapacityProfile {
  StableId profile_id;
  std::uint64_t revision = 0;
  std::uint64_t max_slot_bytes = 0;
  std::uint64_t max_slot_header_bytes = 0;
  std::uint64_t max_continuation_bytes = 0;
  std::uint64_t max_admission_capsule_bytes = 0;
  std::uint64_t max_slot_staging_disk_bytes = 0;
};
// 配置验证先于文件读取/内存分配；未知名字没有无限制后备值。
// Original finite profile rules, shared by Status validation and job creation.
// Pure borrowed shape checks: no diagnostic/string/map allocation.
bool valid_capacity_profile_shape(const CapacityProfile &) noexcept;
bool valid_slot_capacity_profile_shape(const SlotCapacityProfile &) noexcept;
Status validate_capacity_profile(const CapacityProfile &);
Status validate_slot_capacity_profile(const SlotCapacityProfile &);
// 资源错误携带所需数和受信profile版本，始终为技术故障。
Status checkpoint_capacity_failure(const StableId &resource,
                                   std::uint64_t required,
                                   const StableId &profile,
                                   std::uint64_t revision);
// Original code/path/kind construction shared with the job first-error window.
RuleError make_checkpoint_boundary_error(
    std::string_view, std::initializer_list<std::string_view>, ErrorKind);
// Original capacity error fields shared by legacy Status and the job-owned
// first-error window; resource pieces are borrowed until this synchronous call.
std::size_t checkpoint_capacity_diagnostic_bytes(
    std::initializer_list<std::string_view> resource, std::uint64_t required,
    std::string_view profile, std::uint64_t revision);
RuleError
make_checkpoint_capacity_error(std::initializer_list<std::string_view> resource,
                               std::uint64_t required, std::string_view profile,
                               std::uint64_t revision);
// 此工程小样本配置不是全世界发行容量；发行值仍须以实际长期历史测量。
const CapacityProfile &engineering_checkpoint_capacity_profile();
const SlotCapacityProfile &engineering_slot_capacity_profile();
// 一个解码 job 共享原 token；不累计候选内存，wire 与实际容器边界仍独立检查。
struct CodecDecodeBudget {
  const CapacityProfile &profile;
  std::uint64_t candidate_bytes = 0;
  explicit CodecDecodeBudget(const CapacityProfile &value) : profile(value) {}
  // 旧验证工厂仅为兼容调用名，不可重新开启已删除的内存计量。
  static CodecDecodeBudget metered_for_verification(const CapacityProfile &p) {
    return CodecDecodeBudget(p);
  }
  // 显式同 job 接头失败移动 FailureException 到 job 边界。
  CodecDecodeBudget(const CapacityProfile &, CheckpointJobBudget &,
                    CheckpointJobUse = CheckpointJobUse::candidate);
  Status charge_candidate(std::uint64_t bytes);
  Status reject_technical(CheckpointJobFailureCode, std::uint64_t required = 0);
  Status reject_boundary(std::string_view,
                         std::initializer_list<std::string_view>, ErrorKind);
  Status reject_capacity(std::initializer_list<std::string_view> resource,
                         std::uint64_t required);
  Status accept_failure(Status &&);
  bool job_bound() const noexcept { return job_bound_; }
  bool memory_accounting_enabled() const noexcept { return false; }
  // Short-lived decoder metadata retains its own sibling token until
  // destruction.
  CheckpointJobReservation reserve_scratch(std::uint64_t);
  // 实际 allocator 必须属于同一 candidate job；这里只核身份与生命周期。
  Status account_allocator_candidate(
      std::uint64_t bytes, const std::shared_ptr<CheckpointMemoryResource> &);
  // 仍活候选须移交token到其拥有结果；移交后本budget不可继续分配。
  CheckpointJobReservation take_job_reservation() noexcept;
  bool belongs_to(const CheckpointJobBudget &job) const noexcept {
    return job_bound_ && job_reservation_.belongs_to(job);
  }

private:
  CheckpointJobReservation job_reservation_;
  bool job_bound_ = false;
};
} // namespace hanmo::resident
