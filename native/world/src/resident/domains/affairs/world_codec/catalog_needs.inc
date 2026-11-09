// 事务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto affairs =
    decode_affairs_catalog(field(*dynamic, "affairs", "dynamic_world"));
if (!affairs.ok)
  return {false, {}, affairs.error};
if (affairs.value.fiscal_need_policies) {
  if (!settlement_enabled(*result))
    fail("FiscalNeedsRequireSettlement", "dynamic_world/affairs");
  auto checked = validate_fiscal_need_catalog(
      *affairs.value.fiscal_need_policies, *result->fiscal_settlement);
  if (!checked.ok)
    return {false, {}, checked.error};
}
if (affairs.value.cohort_wage_need_policies) {
  auto checked = validate_cohort_wage_need_catalog(
      *affairs.value.cohort_wage_need_policies, *result->local_economy);
  if (!checked.ok)
    return {false, {}, checked.error};
}
result->affairs =
    checkpoint_make_shared<const AffairsCatalog>(std::move(affairs.value));
if (const auto *needs = dynamic->find("occupation_fiscal_needs")) {
  auto parsed = decode_occupation_fiscal_need_profile(*needs);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (!occupation_fiscal_enabled(*result) || !fiscal_needs_enabled(*result))
    fail("OccupationFiscalNeedDependenciesRequired",
         "dynamic_world/occupation_fiscal_needs");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "occupation_fiscal_needs_v1") ==
      caps.end())
    fail("OccupationFiscalNeedCapabilityRequired",
         "dynamic_world/occupation_fiscal_needs");
  result->occupation_fiscal_needs_enabled = true;
}
