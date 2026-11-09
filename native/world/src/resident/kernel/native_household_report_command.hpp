// 原已接受采购计划的告知命令桥；Envelope actor 之外不允许自报接收者或时间。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/interaction/household_purchase_report_model.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct FactRecord;
struct NativeHouseholdPurchaseReportWorldDependencies;
inline constexpr const char *household_report_command_kind =
    "tell_household_purchase_market_report";
struct NativeHouseholdReportCommandQuote {
  // 原 ordinary 字符串复制先取得租约；WorldMoment limbs 沿原 allocator 自计。
  CheckpointMemoryCharge copied_fields;
  WorldIdentity identity;
  StableId actor_id;
  TellHouseholdPurchaseMarketReport command;
  WorldMoment actual_at;
  NativeHouseholdReportCommandQuote() = default;
  NativeHouseholdReportCommandQuote(const WorldIdentity &, const StableId &,
                                    const TellHouseholdPurchaseMarketReport &,
                                    const WorldMoment &);
  NativeHouseholdReportCommandQuote(const NativeHouseholdReportCommandQuote &) =
      default;
  // 精确Moment的值复制可抛，故右值也完整复制；不先搬走原payload租约。
  NativeHouseholdReportCommandQuote(NativeHouseholdReportCommandQuote &&);
  NativeHouseholdReportCommandQuote &
  operator=(NativeHouseholdReportCommandQuote);
  void swap(NativeHouseholdReportCommandQuote &) noexcept;
};
// dependencies 只由中央 compiled owning 提供，绝不是玩家DTO或quote字段。
// phase -> Complete原core/foundation -> report owner，全部词法持有到本次返回。
Outcome<NativeHouseholdReportCommandQuote>
preview_native_household_report_command(
    const WorldBundle &, const StableId &authenticated_actor,
    const TellHouseholdPurchaseMarketReport &,
    const NativeHouseholdPurchaseReportWorldDependencies &);
// 外层原事务拥有整个candidate，任何失败撤整候选。本口不新开保存点。
// 返回已写原Common头借指针，必须在任何candidate变动/提交/回滚前消费；
// 中央Receipt若需要拥有SourceRef，沿其原typed复制租约转换，不长期存裸指针。
Outcome<const FactRecord *> apply_native_household_report_command_in_candidate(
    WorldBundle &, const StableId &authenticated_actor,
    const TellHouseholdPurchaseMarketReport &,
    const NativeHouseholdReportCommandQuote &,
    const NativeHouseholdPurchaseReportWorldDependencies &);
} // namespace hanmo::resident
