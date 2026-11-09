#include "private_shop_household_custody_pins.hpp"
namespace hanmo::resident {
Status visit_private_shop_household_custody_pins(
    const PrivateShopHouseholdCustodyContext &x,
    const MomentOwnerSource &source, PrivateShopHouseholdCustodyPinSink &sink) {
  auto actual = read_private_shop_household_custody(x, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &r = *actual.value;
  const auto *p = x.catalog.policies.find(r.explicit_policy_ref.id);
  if (!p)
    return common_failure("PrivateShopCustodyPolicyRetentionMissing", r.id);
  auto v = sink.original_source(
      "operation_acceptance", 0,
      private_shop_operation_started_source(r.operation_id), 2);
  if (!v.ok)
    return v;
  v = sink.original_source("original_shop_epoch", 0, r.original_shop_epoch,
                           std::nullopt);
  if (!v.ok)
    return v;
  v = sink.policy(r.explicit_policy_ref);
  if (!v.ok)
    return v;
  v = sink.handling_rule(p->handling_rule_id, p->handling_rule_version);
  if (!v.ok)
    return v;
  v = sink.actor(r.proprietor_actor_id);
  if (!v.ok)
    return v;
  v = sink.inventory(r.accepted_service.custody_inventory_id);
  if (!v.ok)
    return v;
  NativePrivateInventoryTitleRead titles(x.market.inventory_titles);
  auto title = titles.read_private_inventory_title_before_source(
      r.accepted_service.custody_inventory_id, source.source_ref);
  if (!title.ok)
    return {false, {}, title.error};
  v = sink.inventory_initial_origin(title.value.initial_origin);
  if (!v.ok)
    return v;
  std::size_t ordinal = 0;
  for (const auto &ref : title.value.original_title_applications) {
    v = sink.original_source("inventory_title_application", ordinal++, ref,
                             std::nullopt);
    if (!v.ok)
      return v;
  }
  return common_success();
}
} // namespace hanmo::resident
