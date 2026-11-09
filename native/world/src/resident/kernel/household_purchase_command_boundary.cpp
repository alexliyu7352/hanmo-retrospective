// 严格成对 JSON 边界，不复制 S5
// 预算、权源或人口执行规则，不创造实际已接受事实。
#include "household_purchase_command_boundary.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"

namespace hanmo::resident {
namespace {
using V = BoundaryValue;
StableId id(const V &value, const StableId &path) {
  auto result = boundary::string(value, path);
  if (result.empty() || result.size() > 256)
    boundary::fail("InvalidHouseholdPurchaseCommandId", path);
  return result;
}
StableId field_id(const V &value, const char *name, const StableId &path) {
  return id(boundary::field(value, name, path), path + "/" + name);
}
void version1(const V &value, const StableId &path) {
  if (boundary::get_int(value, "version", path) != 1)
    boundary::unsupported("UnsupportedHouseholdPurchaseCommandVersion", path);
}
std::vector<HouseholdItemCap> item_caps(const V &value, const StableId &path) {
  const auto &rows = boundary::array(value, path);
  checkpoint_decode_boundary_array(rows.size(), sizeof(HouseholdItemCap));
  std::vector<HouseholdItemCap> out;
  out.reserve(rows.size());
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto at = path + "/" + std::to_string(i);
    boundary::fields(rows[i], {"item_id", "quantity_cap"}, {}, at);
    out.push_back({field_id(rows[i], "item_id", at),
                   boundary::get_int(rows[i], "quantity_cap", at)});
  }
  return out;
}
Status emit_caps(ProjectionWriter &w,
                 const std::vector<HouseholdItemCap> &rows) {
  return w.sequence("allowed_items", rows, [](auto &items, const auto &row) {
    return items.object("", [&](auto &fields) {
      fields.field("item_id", row.item_id);
      return fields.field("quantity_cap", row.quantity_cap);
    });
  });
}
Status emit_ids(ProjectionWriter &w, std::string_view name,
                const std::vector<StableId> &rows) {
  return w.sequence(name, rows, [](auto &items, const auto &row) {
    return items.field("", row);
  });
}
std::vector<StableId> ids(const V &value, const StableId &path) {
  const auto &rows = boundary::array(value, path);
  checkpoint_decode_boundary_array(rows.size(), sizeof(StableId));
  std::vector<StableId> out;
  out.reserve(rows.size());
  for (std::size_t i = 0; i < rows.size(); ++i)
    out.push_back(id(rows[i], path + "/" + std::to_string(i)));
  return out;
}
SourceRef policy_ref(const V &value, const StableId &path) {
  boundary::fields(value, {"domain", "kind", "id"}, {}, path);
  SourceRef out{field_id(value, "domain", path), field_id(value, "kind", path),
                field_id(value, "id", path)};
  if (out.domain != "local_economy" ||
      out.kind != "household_purchase_policy_accepted")
    boundary::fail("HouseholdPurchasePolicyReferenceRequired", path);
  return out;
}
} // namespace

Outcome<HouseholdPurchasePolicyCommand1>
decode_household_purchase_policy_command1(const V &value) {
  auto unique = validate_economic_payload_fields(value);
  if (!unique.ok)
    return {false, {}, std::move(unique.error)};
  try {
    const StableId path = "/household_purchase_policy";
    boundary::fields(value,
                     {"version", "household_id", "allowed_items",
                      "merchandise_cap_qian", "transport_cap_qian",
                      "total_cap_qian", "valid_from_tick", "valid_until_tick",
                      "allowed_market_refs", "allowed_destination_inventories",
                      "allow_cohort_buyer"},
                     {"policy_id"}, path);
    version1(value, path);
    HouseholdPurchasePolicyCommand1 out;
    if (const auto *prior = value.find("policy_id"))
      out.policy_id = id(*prior, path + "/policy_id");
    out.household_id = field_id(value, "household_id", path);
    out.allowed_items = item_caps(boundary::field(value, "allowed_items", path),
                                  path + "/allowed_items");
    out.merchandise_cap_qian =
        boundary::get_int(value, "merchandise_cap_qian", path);
    out.transport_cap_qian =
        boundary::get_int(value, "transport_cap_qian", path);
    out.total_cap_qian = boundary::get_int(value, "total_cap_qian", path);
    out.valid_from_tick = boundary::get_int(value, "valid_from_tick", path);
    out.valid_until_tick = boundary::get_int(value, "valid_until_tick", path);
    out.allowed_market_refs =
        ids(boundary::field(value, "allowed_market_refs", path),
            path + "/allowed_market_refs");
    out.allowed_destination_inventories =
        ids(boundary::field(value, "allowed_destination_inventories", path),
            path + "/allowed_destination_inventories");
    out.allow_cohort_buyer =
        boundary::get_bool(value, "allow_cohort_buyer", path);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Status emit_household_purchase_policy_command1_fields(
    ProjectionWriter &w, const HouseholdPurchasePolicyCommand1 &c) {
  w.field("version", Integer{1});
  w.field("household_id", c.household_id);
  emit_caps(w, c.allowed_items);
  w.field("merchandise_cap_qian", c.merchandise_cap_qian);
  w.field("transport_cap_qian", c.transport_cap_qian);
  w.field("total_cap_qian", c.total_cap_qian);
  w.field("valid_from_tick", c.valid_from_tick);
  w.field("valid_until_tick", c.valid_until_tick);
  emit_ids(w, "allowed_market_refs", c.allowed_market_refs);
  emit_ids(w, "allowed_destination_inventories",
           c.allowed_destination_inventories);
  w.field("allow_cohort_buyer", c.allow_cohort_buyer);
  if (c.policy_id)
    w.field("policy_id", *c.policy_id);
  return w.status();
}
V encode_household_purchase_policy_command1(
    const HouseholdPurchasePolicyCommand1 &c) {
  V out;
  BoundaryProjectionWriter w(out);
  auto valid = w.object("", [&](auto &fields) {
    return emit_household_purchase_policy_command1_fields(fields, c);
  });
  if (valid.ok)
    valid = w.finish();
  if (!valid.ok)
    throw boundary::DecodeFailure{valid.error};
  return out;
}

Outcome<HouseholdPurchasePlanCommand1>
decode_household_purchase_plan_command1(const V &value) {
  auto unique = validate_economic_payload_fields(value);
  if (!unique.ok)
    return {false, {}, std::move(unique.error)};
  try {
    const StableId path = "/household_purchase_plan";
    boundary::fields(value,
                     {"version", "accepted_policy_ref",
                      "accepted_policy_revision", "allowed_items",
                      "spend_cap_qian", "known_market_ref",
                      "destination_inventory", "cohort_id", "latest_start_tick",
                      "delivery_deadline"},
                     {}, path);
    version1(value, path);
    HouseholdPurchasePlanCommand1 out;
    out.accepted_policy_ref =
        policy_ref(boundary::field(value, "accepted_policy_ref", path),
                   path + "/accepted_policy_ref");
    out.accepted_policy_revision =
        boundary::get_int(value, "accepted_policy_revision", path);
    out.allowed_items = item_caps(boundary::field(value, "allowed_items", path),
                                  path + "/allowed_items");
    out.spend_cap_qian = boundary::get_int(value, "spend_cap_qian", path);
    out.known_market_ref = field_id(value, "known_market_ref", path);
    out.destination_inventory = field_id(value, "destination_inventory", path);
    out.cohort_id = field_id(value, "cohort_id", path);
    out.latest_start_tick = boundary::get_int(value, "latest_start_tick", path);
    out.delivery_deadline = boundary::get_int(value, "delivery_deadline", path);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Status emit_household_purchase_plan_command1_fields(
    ProjectionWriter &w, const HouseholdPurchasePlanCommand1 &c) {
  w.field("version", Integer{1});
  w.object("accepted_policy_ref", [&](auto &fields) {
    fields.field("domain", c.accepted_policy_ref.domain);
    fields.field("kind", c.accepted_policy_ref.kind);
    return fields.field("id", c.accepted_policy_ref.id);
  });
  w.field("accepted_policy_revision", c.accepted_policy_revision);
  emit_caps(w, c.allowed_items);
  w.field("spend_cap_qian", c.spend_cap_qian);
  w.field("known_market_ref", c.known_market_ref);
  w.field("destination_inventory", c.destination_inventory);
  w.field("cohort_id", c.cohort_id);
  w.field("latest_start_tick", c.latest_start_tick);
  return w.field("delivery_deadline", c.delivery_deadline);
}
V encode_household_purchase_plan_command1(
    const HouseholdPurchasePlanCommand1 &c) {
  V out;
  BoundaryProjectionWriter w(out);
  auto valid = w.object("", [&](auto &fields) {
    return emit_household_purchase_plan_command1_fields(fields, c);
  });
  if (valid.ok)
    valid = w.finish();
  if (!valid.ok)
    throw boundary::DecodeFailure{valid.error};
  return out;
}
} // namespace hanmo::resident
