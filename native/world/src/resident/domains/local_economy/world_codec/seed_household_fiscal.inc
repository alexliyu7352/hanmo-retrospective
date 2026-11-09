// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (disruption_enabled(catalog)) {
  auto parsed = decode_production_disruption_state(
      field(source, "production_disruptions", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.production_disruptions =
      std::make_shared<const ProductionDisruptionState>(
          std::move(parsed.value));
} else if (source.find("production_disruptions"))
  fail("UnexpectedProductionDisruptionState", "production_disruptions");
if (household_management_enabled(catalog)) {
  auto managed = decode_household_provision_management_state(
      field(source, "household_provision_management", "state"));
  if (!managed.ok)
    return {false, {}, managed.error};
  result.household_provision_management =
      std::make_shared<const HouseholdProvisionManagementState>(
          std::move(managed.value));
} else if (source.find("household_provision_management"))
  fail("UnexpectedHouseholdProvisionManagementState",
       "household_provision_management");
if (catalog.fiscal_taxpayer_continuity_enabled) {
  auto value = decode_fiscal_taxpayer_continuity(
      field(source, "fiscal_taxpayer_continuity", "state"));
  if (!value.ok)
    return {false, {}, value.error};
  result.fiscal_taxpayer_continuity =
      std::make_shared<const FiscalTaxpayerContinuityState>(
          std::move(value.value));
} else if (source.find("fiscal_taxpayer_continuity"))
  fail("UnexpectedFiscalTaxpayerContinuityState", "state");
if (settlement_enabled(catalog)) {
  auto fiscal = decode_fiscal_settlement_state(
      field(source, "fiscal_settlement", "state"));
  if (!fiscal.ok)
    return {false, {}, fiscal.error};
  result.fiscal_settlement =
      std::make_shared<const FiscalSettlementState>(std::move(fiscal.value));
} else if (source.find("fiscal_settlement"))
  fail("UnexpectedFiscalSettlementState", "fiscal_settlement");
