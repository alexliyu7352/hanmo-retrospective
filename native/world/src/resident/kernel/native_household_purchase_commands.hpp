// 原采购政策/计划命令的同 World 桥；只借真实 S5 owner，不另造规则或权源。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "household_purchase_command_boundary.hpp"

namespace hanmo::resident {
struct WorldBundle;
struct FactRecord;
using HouseholdPurchaseOriginalCommand =
    std::variant<HouseholdPurchasePolicyCommand1,
                 HouseholdPurchasePlanCommand1>;
inline constexpr const char *household_purchase_policy_command_kind =
    "accept_household_purchase_policy";
inline constexpr const char *household_purchase_plan_command_kind =
    "plan_household_purchase";
struct NativeHouseholdPurchaseCommandQuote {
  CheckpointMemoryCharge copied_fields;
  WorldIdentity identity;
  StableId actor_id;
  HouseholdPurchaseOriginalCommand command;
  WorldMoment actual_at;
  NativeHouseholdPurchaseCommandQuote() = default;
  NativeHouseholdPurchaseCommandQuote(const WorldIdentity &, const StableId &,
                                      const HouseholdPurchaseOriginalCommand &,
                                      const WorldMoment &);
  NativeHouseholdPurchaseCommandQuote(
      const NativeHouseholdPurchaseCommandQuote &) = default;
  NativeHouseholdPurchaseCommandQuote(NativeHouseholdPurchaseCommandQuote &&);
  NativeHouseholdPurchaseCommandQuote &
  operator=(NativeHouseholdPurchaseCommandQuote);
  void swap(NativeHouseholdPurchaseCommandQuote &) noexcept;
};
Outcome<NativeHouseholdPurchaseCommandQuote>
preview_native_household_purchase_command(
    const WorldBundle &, const StableId &actor,
    const HouseholdPurchaseOriginalCommand &);
// 外层必须拥有完整候选事务；任何晚失败均撤销整候选，不在这里开嵌套保存点。
Outcome<const FactRecord *>
apply_native_household_purchase_command_in_candidate(
    WorldBundle &, const StableId &actor,
    const HouseholdPurchaseOriginalCommand &,
    const NativeHouseholdPurchaseCommandQuote &);
} // namespace hanmo::resident
