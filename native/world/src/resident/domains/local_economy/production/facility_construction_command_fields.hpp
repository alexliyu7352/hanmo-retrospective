// 原输入字段序唯一出口；规则/精确选批委托本域已存在字段，不另造参数协议。
#pragma once
#include "facility_construction_commands.hpp"
#include "facility_construction_fields.hpp"
namespace hanmo::resident::facility_construction_command_fields {
template <class C> Status plan(C &c, PlanFacilityCapacity &value) {
  facility_construction_fields::begin(c);
  exact_fields::required_ref(c, "asset_ref", value.asset_ref);
  c.object("upgrade_spec", [&](auto &out) {
    return facility_construction_fields::rule(out, value.upgrade_spec);
  });
  c.field("workers", value.workers);
  return c.sequence(
      "materials", value.materials, [](auto &out, auto &selection) {
        return foundation_asset_resource_fields::selection(out, selection);
      });
}
} // namespace hanmo::resident::facility_construction_command_fields
