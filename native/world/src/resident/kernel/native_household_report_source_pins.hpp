// 同 World 告知的派生来源依赖；不是 checkpoint 内存 census，也不是持久
// refcount。
#pragma once
#include "resident/domains/actor_foundation/character_availability_retention_refs.hpp"
#include "resident/domains/actor_foundation/character_availability_source_retention_refs.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <variant>
namespace hanmo::resident {
struct WorldBundle;
struct NativeCompleteWorldCoreSources;
class NativeHouseholdFoundationLease;
class NativeHouseholdPurchaseReportWorldRead;
// 固定消费者槽与真实拥有表是两种身份；目录/原行不能伪装为 Common 事实。
enum class NativeHouseholdReportDependencySlot {
  actual_delivery,
  actual_contact_prefix,
  controller_history_prefix,
  plan,
  policy,
  population_selection,
  population_materialization,
  buyer_labor,
  population_initial,
  population_revision,
  report,
  knowledge,
  knowledge_time,
  report_time,
  message_created_time,
  message_delivered_time,
  message_execution,
  sender_knowledge,
  sender_knowledge_time,
  social_time_source,
  pending_message,
  message_business,
  message_delivery,
  dossier_access,
  original_observation,
  original_permission,
  actor_location,
  population_location
};
using NativeHouseholdReportPinSlot =
    std::variant<NativeHouseholdReportDependencySlot,
                 CharacterAvailabilityProvenanceSlot>;
enum class NativeHouseholdReportRowOwner {
  household_plans,
  household_policies,
  population_selections,
  population_materializations,
  buyer_labor,
  authored_cohorts,
  current_cohorts,
  population_revisions,
  population_heads,
  population_transport_bindings,
  population_transfers,
  population_losses,
  social_reports,
  social_knowledge,
  social_time_bindings,
  social_message_execution,
  social_messages,
  communication_business,
  communication_delivery,
  communication_dossier
};
// root/row/key 仅由下方唯一 collector
// 对实际表地址核准后构造；任何根写入即失效。 此类型没有释放权限。实际原 owner
// 未提供 release 时不得据此放行。
struct NativeHouseholdReportOwnedRowPin {
  NativeHouseholdReportRowOwner owner{};
  const void *root = nullptr;
  const void *row = nullptr;
  const StableId *key = nullptr;
  std::size_t ordinal = 0;
};
struct NativeHouseholdReportCommonFactPin {
  const CommonStore *owner = nullptr;
  const FactRecord *row = nullptr;
  std::size_t actual_position = 0;
};
// 只有原 full reader 给出的 schema 才可填写；无统一 schema 的原行保持 typed。
struct NativeHouseholdReportNamedSourcePin {
  SourceRef source;
  Integer actual_schema = 0;
};
using NativeHouseholdReportDependency = std::variant<
    NativeHouseholdReportOwnedRowPin, NativeHouseholdReportCommonFactPin,
    NativeHouseholdReportNamedSourcePin, CharacterAvailabilityRetentionRef,
    CharacterAvailabilitySourceRetentionRef>;
struct NativeHouseholdReportActualPin {
  const HouseholdPurchaseReportDisclosure *consumer = nullptr;
  NativeHouseholdReportPinSlot slot;
  bool applied_before_cut = true;
  NativeHouseholdReportDependency dependency;
};
class NativeHouseholdReportPinCollector;
// 同一个不可变 read job 的借用索引。source 字符串先由首字段租约预留，
// 原行始终只借 World/Prepared 真根；clone/decode/rollback/任何写入后必须重建。
class NativeHouseholdReportSourcePins {
  friend class NativeHouseholdReportPinCollector;
  CheckpointMemoryCharge payload_memory_;
  const WorldBundle *world_ = nullptr;
  CheckpointVector<NativeHouseholdReportActualPin> pins_;

public:
  NativeHouseholdReportSourcePins() = default;
  NativeHouseholdReportSourcePins(const NativeHouseholdReportSourcePins &) =
      delete;
  NativeHouseholdReportSourcePins &
  operator=(const NativeHouseholdReportSourcePins &) = delete;
  NativeHouseholdReportSourcePins(NativeHouseholdReportSourcePins &&) noexcept =
      default;
  NativeHouseholdReportSourcePins &
  operator=(NativeHouseholdReportSourcePins &&) = delete;
  const CheckpointVector<NativeHouseholdReportActualPin> &
  pins() const noexcept {
    return pins_;
  }
  bool bound_to(const WorldBundle &world) const noexcept {
    return world_ == &world;
  }
  // 此查询只证明该实际消费者依赖，false 不是释放许可，仍须全消费者与原 owner
  // gate。
  bool contains_source(SourceRefBuildView) const noexcept;
  bool contains_source(const SourceRef &ref) const noexcept {
    return contains_source(SourceRefBuildView{ref.domain, ref.kind, ref.id});
  }
};
// 真实 full S3 源 + 整 Common 前缀 + controller 全历史统一进入同一 collector。
// 不在工厂构造中调用，不重开 Complete/P10，不把有行当作其业务来源已验证。
Outcome<NativeHouseholdReportSourcePins>
collect_native_household_report_source_pins(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeHouseholdFoundationLease &,
    const NativeHouseholdPurchaseReportWorldRead &);
// 交给原 Growth 回收器的拥有型 ID 列表。借读图在返回前销毁，之后才准写原根。
// 首租约覆盖原 STL vector 与字符串；nullopt 只代表没有选 Face3。
struct NativeHouseholdReportGrowthPins {
  CheckpointMemoryCharge memory;
  std::vector<StableId> ids;
  NativeHouseholdReportGrowthPins() = default;
  NativeHouseholdReportGrowthPins(const NativeHouseholdReportGrowthPins &) =
      delete;
  NativeHouseholdReportGrowthPins &
  operator=(const NativeHouseholdReportGrowthPins &) = delete;
  NativeHouseholdReportGrowthPins(NativeHouseholdReportGrowthPins &&) noexcept =
      default;
  NativeHouseholdReportGrowthPins &
  operator=(NativeHouseholdReportGrowthPins &&) = delete;
};
// 不替代 Growth 自己的 active/原责任门，只给原实际删除入口增加本域真实引用。
// 失败不改任何原表；外层仍负责整候选回滚，不可只忽略失败继续回收。
Outcome<std::optional<NativeHouseholdReportGrowthPins>>
read_native_household_report_growth_pins(const WorldBundle &,
                                         const std::vector<StableId> &existing);
} // namespace hanmo::resident
