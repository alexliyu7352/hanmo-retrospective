// 只表达一次家户采购授权或计划意图；真实权源、执行事实由原 S5 owner 填入。
#pragma once
#include "resident/domains/local_economy/household_purchase_types.hpp"

namespace hanmo::resident {
struct BoundaryValue;
class ProjectionWriter;
struct HouseholdPurchasePolicyCommand1 {
  std::optional<StableId> policy_id;
  StableId household_id;
  std::vector<HouseholdItemCap> allowed_items;
  Integer merchandise_cap_qian = 0, transport_cap_qian = 0, total_cap_qian = 0;
  Integer valid_from_tick = 0, valid_until_tick = 0;
  std::vector<StableId> allowed_market_refs, allowed_destination_inventories;
  bool allow_cohort_buyer = false;
};
struct HouseholdPurchasePlanCommand1 {
  SourceRef accepted_policy_ref;
  Integer accepted_policy_revision = 0;
  std::vector<HouseholdItemCap> allowed_items;
  Integer spend_cap_qian = 0;
  StableId known_market_ref, destination_inventory, cohort_id;
  Integer latest_start_tick = 0, delivery_deadline = 0;
};
Status emit_household_purchase_policy_command1_fields(
    ProjectionWriter &, const HouseholdPurchasePolicyCommand1 &);
Status emit_household_purchase_plan_command1_fields(
    ProjectionWriter &, const HouseholdPurchasePlanCommand1 &);
// /1 仅接受有限原意字段；Actor 来自 Envelope，不接受 proof/人口选择/到场自报。
Outcome<HouseholdPurchasePolicyCommand1>
decode_household_purchase_policy_command1(const BoundaryValue &);
BoundaryValue encode_household_purchase_policy_command1(
    const HouseholdPurchasePolicyCommand1 &);
Outcome<HouseholdPurchasePlanCommand1>
decode_household_purchase_plan_command1(const BoundaryValue &);
BoundaryValue
encode_household_purchase_plan_command1(const HouseholdPurchasePlanCommand1 &);
} // namespace hanmo::resident
