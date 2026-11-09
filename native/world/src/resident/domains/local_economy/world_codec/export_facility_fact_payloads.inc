// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  auto headers = validate_native_economic_facility_headers(
      w.common, *w.catalog, &w.local_economy,
      w.economic_facility_capacity_sources
          ? &*w.economic_facility_capacity_sources
          : nullptr,
      w.production_facility_conditions ? &*w.production_facility_conditions
                                       : nullptr,
      w.governance_work_expense ? &*w.governance_work_expense : nullptr);
  if (!headers.ok)
    return headers;
}
if (w.production_facility_conditions)
  WORLD_FIELDS(P::fact_payloads, "production_facility_conditions",
               emit_native_economic_facility_fact_payloads_fields(
                   out, w.common, *w.catalog, &w.local_economy,
                   &*w.economic_facility_capacity_sources,
                   &*w.production_facility_conditions,
                   w.governance_work_expense ? &*w.governance_work_expense
                                             : nullptr,
                   NativeEconomicFacilityFactProjection::conditions));
if (w.governance_work_expense)
  WORLD_FIELDS(P::fact_payloads, "governance_work_expense",
               emit_native_economic_facility_fact_payloads_fields(
                   out, w.common, *w.catalog, &w.local_economy,
                   w.economic_facility_capacity_sources
                       ? &*w.economic_facility_capacity_sources
                       : nullptr,
                   w.production_facility_conditions
                       ? &*w.production_facility_conditions
                       : nullptr,
                   &*w.governance_work_expense,
                   NativeEconomicFacilityFactProjection::work));
