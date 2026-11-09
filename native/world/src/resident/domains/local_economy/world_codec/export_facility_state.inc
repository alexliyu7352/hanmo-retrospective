// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto facility_layout = validate_native_economic_facility_layout(w);
if (!facility_layout.ok)
  return facility_layout;
if (w.economic_facility_capacity_sources)
  WORLD_WHOLE(P::section, "economic_facility_capacity_sources",
              emit_economic_facility_sources(
                  out, *w.economic_facility_capacity_sources));
if (w.production_facility_conditions)
  WORLD_WHOLE(P::section, "production_facility_conditions",
              emit_production_facility_conditions(
                  out, *w.production_facility_conditions));
if (w.governance_work_expense)
  WORLD_WHOLE(P::section, "governance_work_expense",
              emit_governance_work_expenses(out, *w.governance_work_expense));
