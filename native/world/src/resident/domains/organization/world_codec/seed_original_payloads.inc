// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore original(result.common, catalog.common);
  auto payloads = projection_value([&](auto &out) {
    return emit_native_economic_facility_fact_payloads_fields(
        out, original, catalog, result.local_economy.get(),
        result.economic_facility_capacity_sources.get(),
        result.production_facility_conditions.get(),
        result.governance_work_expense.get(),
        NativeEconomicFacilityFactProjection::all);
  });
  if (!payloads.ok)
    return {false, {}, std::move(payloads.error)};
  for (const auto &row : object(payloads.value, "economic_facility/facts")) {
    const auto &actual =
        field(field(source, "facts", "state"), row.first, "facts");
    if (!compare_boundary(actual, row.second).exact())
      fail("EconomicFacilityOriginalFactPayloadMismatch", row.first);
  }
}
