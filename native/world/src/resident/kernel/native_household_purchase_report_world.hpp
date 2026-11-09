// S7 计划告知的同 World 原源拥有域；只组装原消息、空间和知情读口，不复制世界。
#pragma once
#include "native_complete_world_core.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include <memory>
namespace hanmo::resident {
class NativeHouseholdFoundationLease;
class CommonActualFactPrefixSink;
class CharacterAvailabilityProvenanceSink;
// 本原实际消息和其不可删改的 Common 前缀由中央唯一 retention collector 消费。
// ordinal 是原 Common 顺序；不另存引用计数，也不在这里打开 P10/全世界租约。
enum class NativeHouseholdReportPinRole {
  actual_delivery,
  actual_contact_prefix
};
class NativeHouseholdReportPinSink {
public:
  virtual ~NativeHouseholdReportPinSink() = default;
  virtual Status original_source(const StableId &delivery_id,
                                 NativeHouseholdReportPinRole, Integer ordinal,
                                 const SourceRefBuildView &) = 0;
};
Status visit_native_household_purchase_report_source_pins(
    const WorldBundle &, NativeHouseholdReportPinSink &);
struct TaskReportCarrierOfferCatalog;
// 来源由中央已编译选择给出；shared_ptr 必须保活真实原 owner，不接无主局部别名。
// 公开市场报告不借业务扩权；非公开报告没有实际许可 owner 时原读明确拒绝。
struct NativeHouseholdPurchaseReportWorldDependencies {
  ReadStampLimits stamp_limits;
  std::shared_ptr<const ReportDisclosureOriginalRead> disclosure_originals;
  std::shared_ptr<const ReportOfficialContentRead> official_contents;
  std::shared_ptr<const TaskReportCarrierOfferCatalog> carrier_offers;
  std::shared_ptr<const NamedMilitaryCustodyPlacementRead> custody_releases;
};
// core 与 foundation 由同一外层 Complete/domain owner 持有并先构造、后销毁。
// 本域拥有全部局部 contexts/P10 快照及渠道适配器；不通过保活外层形成自环。
class NativeHouseholdPurchaseReportWorldRead final
    : public CheckpointAllocatedObject {
  struct Sources;
  std::unique_ptr<Sources> sources_;
  explicit NativeHouseholdPurchaseReportWorldRead(std::unique_ptr<Sources>);

public:
  ~NativeHouseholdPurchaseReportWorldRead();
  NativeHouseholdPurchaseReportWorldRead(
      const NativeHouseholdPurchaseReportWorldRead &) = delete;
  NativeHouseholdPurchaseReportWorldRead &
  operator=(const NativeHouseholdPurchaseReportWorldRead &) = delete;
  NativeHouseholdPurchaseReportWorldRead(
      NativeHouseholdPurchaseReportWorldRead &&) = delete;
  NativeHouseholdPurchaseReportWorldRead &
  operator=(NativeHouseholdPurchaseReportWorldRead &&) = delete;
  static Outcome<std::unique_ptr<NativeHouseholdPurchaseReportWorldRead>>
  create(const WorldBundle &, const NativeCompleteWorldCoreSources &,
         const NativeHouseholdFoundationLease &,
         NativeHouseholdPurchaseReportWorldDependencies);
  bool bound_to(const WorldBundle &,
                const NativeHouseholdFoundationLease &) const noexcept;
  Status validate_bindings() const;
  Status validate_sources() const;
  // 对一个实际告知保全原控制者历史依赖，直接复用原两种 typed sink。
  // 不重建 Complete/P10，不猜无注册 SourceRef 的出生/身份行；后缀虽未在
  // 当时应用也不可丢。sink 不得改根，任一失败丢弃本次所有输出。
  // 此口只证明/遍历控制者历史，不替代市场报告、知情、空间及完整冷验。
  Status
  visit_controller_history_pins(const SourceRef &actual_delivery,
                                CommonActualFactPrefixSink &,
                                CharacterAvailabilityProvenanceSink &) const;
  Outcome<const HouseholdPurchaseReportContext *> context() const;
};
} // namespace hanmo::resident
