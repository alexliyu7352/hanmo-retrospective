// 有限原目录预算身份；只供实际选择目录值的生产者使用，不接收作者自定域。
#pragma once
namespace hanmo::resident {
enum class CheckpointCatalogJsonKind : int {
  public_supply_inspection,
  growth_permanent_definitions,
  dynamic_military_action_timings,
  military_spatial_definitions,
  military_session_definitions,
  item_services,
  repair_policies,
  repair_service_rules,
  craft_recipes,
  organization_lifecycle,
  organization_authority,
  local_economy,
  private_shops,
  estate_employment,
  estate,
  economic_facilities,
  production_facility_conditions,
  governance_work_fees,
  governance_program_catalog,
  military_duty_catalog,
  dependent_provision_catalog,
  facility_claims,
  household_purchases,
  household_retail_settlements,
  production_cycles,
  politics,
  fiscal_settlement,
  private_shop_household_custody,
  political_treaty_terms,
  treaty_authority_mapping,
  political_relations
};
} // namespace hanmo::resident
