// 明确托管接受的真实来源索引；不新建持久引用计数或资源账。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/actor_foundation/character_availability_retention_refs.hpp"
#include "resident/domains/actor_foundation/character_availability_source_retention_refs.hpp"
#include "resident/domains/local_economy/private_shop_household_custody_pins.hpp"
#include <variant>
namespace hanmo::resident {
struct WorldBundle;
struct NativeCompleteWorldCoreSources;
struct HistoryRecord;
struct CommitResult;
class NativeHouseholdFoundationLease;
enum class NativePrivateShopCustodyPinSlot {
  accepted,
  operation,
  operation_input,
  opening,
  opening_catalog,
  opening_growth,
  opening_task,
  opening_permission,
  opening_availability,
  policy,
  handling_rule,
  actor,
  inventory,
  initial_origin,
  title_application,
  title_checked_history
};
enum class NativePrivateShopCustodyRowOwner {
  accepted,
  operation,
  operation_acceptance,
  operation_profile,
  shop,
  premises,
  policy,
  handling_rule,
  market_definition,
  capability_purpose,
  actor,
  inventory,
  wallet,
  required_inventory,
  authored_actor,
  foundation_catalog_definition,
  common_catalog_definition,
  family_catalog_definition,
  family_state_identity,
  family_birth_reply,
  authored_actor_id,
  character_origin,
  registration,
  condition_seed,
  scenario_origin,
  construction_receipt,
  actor_age_basis,
  actor_domain_binding,
  estate_definition,
  estate_current,
  family_fact,
  family_succession_order,
  military_occupation,
  naval_capture,
  naval_vessel,
  place,
  role_definition,
  dialogue,
  dialogue_node,
  rent_rate,
  original_market,
  family_prototype,
  actor_initial_birth,
  growth_study,
  birth,
  common_action,
  task,
  task_contract,
  task_definition,
  task_frozen_definition,
  task_transfer,
  task_delivery,
  task_escort,
  task_static_payment,
  task_static_asset,
  task_message_handoff,
  task_message_report,
  task_exact_start,
  task_exact_terminal,
  task_shop_binding,
  task_shop_settlement,
  task_shop_action,
  task_shop_interruption,
  task_shop_estate_cancellation,
  task_personal_message_start,
  task_personal_message_end,
  task_personal_report_start,
  task_personal_report_end,
  task_observer_index,
  organization_id,
  licensing_court,
  licensing_registry,
  licensing_warrant,
  licensing_release,
  licensing_clearance,
  licensing_permit,
  licensing_revocation,
  jurisdiction_initial,
  jurisdiction_current,
  office_initial,
  office_current,
  organization_fact,
  member_initial,
  member_current,
  member_policy,
  membership_offer,
  membership_decision,
  membership_terms,
  grant_initial,
  grant_policy,
  grant_current,
  takeover_rule,
  takeover_registration,
  military_stronghold_definition,
  military_garrison_definition,
  military_siege,
  military_army,
  military_campaign,
  military_order_definition,
  military_item_id
};
// root/row/key 是已验证的真实表身份；vector 使用原序号，不给目录捏造事实
// schema。
struct NativePrivateShopCustodyOwnedRowPin {
  NativePrivateShopCustodyRowOwner owner{};
  const void *root = nullptr;
  const void *row = nullptr;
  const StableId *key = nullptr;
  std::size_t ordinal = 0;
};
// 只保原时钟/实例分配上界的拥有字段，不等于钉住Common全部事实或缓存。
struct NativePrivateShopCustodyCommonBoundaryPin {
  const CommonStore *owner = nullptr;
  const WorldIdentity *identity = nullptr;
  const Integer *next_instance_id = nullptr;
};
struct NativePrivateShopCustodyCommonPin {
  const CommonStore *owner = nullptr;
  const FactRecord *row = nullptr;
  std::size_t position = 0;
};
// 仅原 operation source visitor 提供的真实 schema 可以进入此分支。
struct NativePrivateShopCustodyCommandPin {
  const CommonStore *owner = nullptr;
  const CommitResult *result = nullptr;
  const HistoryRecord *history = nullptr;
};
struct NativePrivateShopCustodyNamedPin {
  SourceRef source;
  Integer actual_schema = 0;
};
// 原十一槽保独立分型；不是把typed对象名改造成事实kind/schema。
struct NativePrivateShopCustodyAvailabilityPin {
  CharacterAvailabilityProvenanceSlot slot{};
  std::variant<CharacterAvailabilityRetentionRef,
               CharacterAvailabilitySourceRetentionRef>
      source;
};
using NativePrivateShopCustodyDependency = std::variant<
    NativePrivateShopCustodyOwnedRowPin, NativePrivateShopCustodyCommonPin,
    NativePrivateShopCustodyCommonBoundaryPin, NativePrivateShopCustodyNamedPin,
    NativePrivateShopCustodyCommandPin,
    NativePrivateShopCustodyAvailabilityPin>;
struct NativePrivateShopCustodyActualPin {
  const PrivateShopHouseholdCustodyAcceptedSource *consumer = nullptr;
  NativePrivateShopCustodyPinSlot slot{};
  const FactRecord *actual_cut = nullptr;
  bool applied_before_cut = true;
  NativePrivateShopCustodyDependency dependency;
};
class NativePrivateShopCustodyPinCollector;
// 源字符串租约先于 pins 析构；表地址只在同一个未改写的 World read job 内有效。
class NativePrivateShopCustodySourcePins {
  friend class NativePrivateShopCustodyPinCollector;
  CheckpointMemoryCharge memory_;
  const WorldBundle *world_ = nullptr;
  CheckpointVector<NativePrivateShopCustodyActualPin> pins_;

public:
  NativePrivateShopCustodySourcePins() = default;
  NativePrivateShopCustodySourcePins(
      const NativePrivateShopCustodySourcePins &) = delete;
  NativePrivateShopCustodySourcePins &
  operator=(const NativePrivateShopCustodySourcePins &) = delete;
  NativePrivateShopCustodySourcePins(
      NativePrivateShopCustodySourcePins &&) noexcept = default;
  NativePrivateShopCustodySourcePins &
  operator=(NativePrivateShopCustodySourcePins &&) = delete;
  bool bound_to(const WorldBundle &w) const noexcept { return world_ == &w; }
  const CheckpointVector<NativePrivateShopCustodyActualPin> &
  pins() const noexcept {
    return pins_;
  }
  // false 不提供释放许可；全消费者集合与原 owner 的 release gate 仍独立必需。
  bool contains_source(SourceRefBuildView) const noexcept;
};
// 外层已经持有真实 foundation；这里只借同根，不回 Complete 工厂或产生第二图。
Outcome<NativePrivateShopCustodySourcePins>
collect_native_private_shop_custody_source_pins(
    const WorldBundle &, const NativeCompleteWorldCoreSources &,
    const NativeHouseholdFoundationLease &);
// 原 Growth 删除入口使用的拥有型 ID；没有借读图或原表指针逃逸到写阶段。
struct NativePrivateShopCustodyGrowthPins {
  CheckpointMemoryCharge memory;
  std::vector<StableId> ids;
  NativePrivateShopCustodyGrowthPins() = default;
  NativePrivateShopCustodyGrowthPins(
      const NativePrivateShopCustodyGrowthPins &) = delete;
  NativePrivateShopCustodyGrowthPins &
  operator=(const NativePrivateShopCustodyGrowthPins &) = delete;
  NativePrivateShopCustodyGrowthPins(
      NativePrivateShopCustodyGrowthPins &&) noexcept = default;
  NativePrivateShopCustodyGrowthPins &
  operator=(NativePrivateShopCustodyGrowthPins &&) = delete;
};
Outcome<std::optional<NativePrivateShopCustodyGrowthPins>>
read_native_private_shop_custody_growth_pins(
    const WorldBundle &, const std::vector<StableId> &existing);
} // namespace hanmo::resident
