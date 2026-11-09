#pragma once
#include "private_shop_household_custody.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resources/resource_site_recovery_fields.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {


} // namespace hanmo::resident
namespace hanmo::resident::private_shop_household_custody_fields {
using namespace exact_fields;
using resource_recovery_fields::begin;
// 三种原 visitor 共用行字段；二进制沿 Common 的有序 key/record 表算法。
template <class C, class T, class Fields>
Status table(C &c, std::string_view name, OrderedTable<T> &rows, Fields fields) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    return common_checkpoint::table(c, name, rows, [&](auto &out, auto &row) {
      return out.object("record", [&](auto &record) {
        return fields(record, row);
      });
    });
  else
    return c.table(name, rows, fields);
}
template <class C> Status service(C &c, HouseholdMarketServiceDefinition &v) {
  begin(c);
  required_ref(c, "id", v.id);
  required_ref(c, "market_ref", v.market_ref);
  required_ref(c, "place_id", v.place_id);
  required_ref(c, "custody_inventory_id", v.custody_inventory_id);
  c.object("service_slot", [&](auto &x) {
    begin(x);
    required_ref(x, "provider_domain", v.service_slot.provider_domain);
    required_ref(x, "facility_id", v.service_slot.facility_id);
    return required_ref(x, "slot_kind", v.service_slot.slot_kind);
  });
  c.object("custody_terms_ref", [&](auto &x) {
    begin(x);
    return source(x, v.custody_terms_ref);
  });
  c.object("service_usage_right_ref", [&](auto &x) {
    begin(x);
    return source(x, v.service_usage_right_ref);
  });
  c.object("handling_rule_ref", [&](auto &x) {
    begin(x);
    return source(x, v.handling_rule_ref);
  });
  return c.status();
}
template <class C> Status policy(C &c, PrivateShopHouseholdCustodyPolicy &v) {
  begin(c);
  c.object("policy_ref", [&](auto &x) {
    return resource_recovery_fields::rule_ref(x, v.policy_ref);
  });
  required_ref(c, "premises_id", v.premises_id);
  Integer selection = static_cast<Integer>(v.inventory_selection);
  c.field("inventory_selection", selection);
  if (selection != 0)
    return c.reject("UnsupportedPrivateShopCustodyInventorySelection");
  if (c.reading())
    v.inventory_selection =
        PrivateShopCustodyInventorySelection::proprietor_personal_inventory;
  required_ref(c, "handling_rule_id", v.handling_rule_id);
  c.field("handling_rule_version", v.handling_rule_version);
  if (v.handling_rule_version != 1)
    return c.reject("UnsupportedPrivateShopCustodyHandlingRuleVersion");
  return c.status();
}
template <class C>
Status accepted(C &c, PrivateShopHouseholdCustodyAcceptedSource &v) {
  begin(c);
  required_ref(c, "id", v.id);
  required_ref(c, "operation_id", v.operation_id);
  required_ref(c, "proprietor_actor_id", v.proprietor_actor_id);
  c.object("original_shop_epoch", [&](auto &x) {
    begin(x);
    return source(x, v.original_shop_epoch);
  });
  c.object("explicit_policy_ref", [&](auto &x) {
    return resource_recovery_fields::rule_ref(x, v.explicit_policy_ref);
  });
  c.object("accepted_at", [&](auto &x) {
    begin(x);
    return moment(x, v.accepted_at);
  });
  c.object("accepted_service",
           [&](auto &x) { return service(x, v.accepted_service); });
  return c.status();
}
template <class C> Status root(C &c, PrivateShopHouseholdCustodyState &v) {
  begin(c);
  c.field("version", v.version);
  if (v.version != 1)
    return c.reject("UnsupportedPrivateShopCustodyVersion");
  table(c, "accepted", v.accepted,
          [](auto &x, auto &r) { return accepted(x, r); });
  return c.status();
}
template <class C> Status catalog(C &c, PrivateShopHouseholdCustodyCatalog &v) {
  begin(c);
  c.field("version", v.version);
  if (v.version != 1)
    return c.reject("UnsupportedPrivateShopCustodyCatalogVersion");
  required_ref(c, "catalog_digest", v.catalog_digest);
  table(c, "policies", v.policies,
          [](auto &x, auto &r) { return policy(x, r); });
  return c.status();
}
} // namespace hanmo::resident::private_shop_household_custody_fields
