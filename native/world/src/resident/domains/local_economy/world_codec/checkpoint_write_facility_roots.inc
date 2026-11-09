// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_economic_facility_layout(w);
if (!r.ok)
  return r;
if (w.economic_facility_capacity_sources) {
  r = section("economic_facility_capacity_sources", [&](auto &c) {
    return visit_economic_facility_sources(
        c, *w.economic_facility_capacity_sources);
  });
  if (!r.ok)
    return r;
}
if (w.production_facility_conditions) {
  r = section("production_facility_conditions", [&](auto &c) {
    return visit_production_facility_conditions(
        c, *w.production_facility_conditions);
  });
  if (!r.ok)
    return r;
}
if (w.governance_work_expense) {
  r = section("governance_work_expense", [&](auto &c) {
    return visit_governance_work_expenses(c, *w.governance_work_expense);
  });
  if (!r.ok)
    return r;
}
