// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore view(result.common, catalog.common);
  auto payloads = apprenticeship_fact_payloads(
      view, catalog, result.family_apprenticeship.get());
  if (!payloads.ok)
    return {false, {}, payloads.error};
  for (const auto &row :
       object(payloads.value, "family_apprenticeship/facts")) {
    const auto &original =
        field(field(source, "facts", "state"), row.first, "facts");
    if (!compare_boundary(original, row.second).exact())
      fail("ApprenticeshipFactPayloadMismatch", row.first);
  }
}
