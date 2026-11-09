// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("fiscal_taxpayer_continuity")) {
  auto parsed = decode_fiscal_taxpayer_continuity_profile(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (!takeover_enabled(*result) || !settlement_enabled(*result) ||
      !political_fiscal_enabled(*result))
    fail("FiscalTaxpayerContinuityDependenciesRequired",
         "dynamic_world/fiscal_taxpayer_continuity");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "fiscal_taxpayer_continuity_v1") ==
      caps.end())
    fail("FiscalTaxpayerContinuityCapabilityRequired",
         "dynamic_world/fiscal_taxpayer_continuity");
  result->fiscal_taxpayer_continuity_enabled = true;
}
if (const auto *config = dynamic->find("political_occupation_fiscal")) {
  if (!takeover_enabled(*result) || !political_fiscal_enabled(*result) ||
      !settlement_enabled(*result))
    fail("OccupationFiscalDependenciesRequired",
         "dynamic_world/political_occupation_fiscal");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "occupation_fiscal_v1") == caps.end())
    fail("OccupationFiscalCapabilityRequired",
         "dynamic_world/political_occupation_fiscal");
  auto parsed = decode_occupation_fiscal_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (!parsed.value.enabled)
    fail("OccupationFiscalProfileRequired",
         "dynamic_world/political_occupation_fiscal");
  result->occupation_fiscal =
      checkpoint_make_shared<const OccupationFiscalCatalog>(
          std::move(parsed.value));
  result->occupation_fiscal_settlement_enabled = true;
}
