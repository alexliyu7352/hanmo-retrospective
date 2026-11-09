# 地方经济与责任账：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/local_economy/obligations/transport_original_obligations.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_transport_original_obligations_cpp
    src/resident/domains/local_economy/obligations/transport_original_obligations.cpp
    src/resident/domains/local_economy/obligations/material_aid_original_obligations.cpp
    src/resident/domains/local_economy/material_aid/material_aid_read.cpp
    src/resident/domains/local_economy/material_aid/material_aid.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/service_finance_execution.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_service_finance_execution_cpp
    src/resident/domains/local_economy/service_finance_execution.cpp
    src/resident/domains/local_economy/obligations/obligation_sources_read.cpp
    src/resident/domains/local_economy/obligations/obligation_sources_pins.cpp
    src/resident/domains/local_economy/obligations/obligation_sources_codec.cpp
    src/resident/domains/local_economy/obligations/native_service_obligation_sources.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/private_account_dependencies.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_private_account_dependencies_cpp
    src/resident/domains/local_economy/private_account_dependencies.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/private_account_history.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_private_account_history_cpp
    src/resident/domains/local_economy/private_account_history.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/takeover/execution_configurations.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_execution_configurations_cpp
    src/resident/domains/local_economy/takeover/execution_configurations.cpp
    src/resident/domains/local_economy/takeover/execution_materials.cpp
    src/resident/domains/local_economy/takeover/execution_phase.cpp
    src/resident/domains/local_economy/takeover/execution_receipts.cpp
    src/resident/domains/local_economy/takeover/execution_receipts_codec.cpp
    src/resident/domains/local_economy/takeover/responsibility_execution.cpp
    src/resident/domains/local_economy/takeover/responsibility_source.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/household/provision_management.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_provision_management_cpp
    src/resident/domains/local_economy/household/provision_management.cpp
    src/resident/domains/local_economy/household/provision_management_codec.cpp
    src/resident/domains/local_economy/household/provision_management_view.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions_codec.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions_view.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/fiscal/fiscal_settlement.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_fiscal_settlement_cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_codec.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_rules.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_debt_source.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_debt_source.cpp
    src/resident/domains/local_economy/fiscal/fiscal_need_adapter.cpp
    src/resident/domains/local_economy/wages/cohort_wage_source.cpp
    src/resident/domains/local_economy/wages/cohort_wage_need_adapter.cpp
    src/resident/domains/local_economy/wages/cohort_wage_need_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_need_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_codec.cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_view.cpp
    src/resident/domains/local_economy/relief/public_relief_rules.cpp
    src/resident/domains/local_economy/relief/public_relief_codec.cpp
    src/resident/domains/local_economy/relief/public_relief_catalog_links.cpp
    src/resident/domains/local_economy/local_economy_checkpoint.cpp
    src/resident/domains/local_economy/local_economy_codec.cpp
    src/resident/domains/local_economy/local_economy_rules.cpp
    src/resident/domains/local_economy/household_provisioning.cpp
    src/resident/domains/local_economy/views/local_economy_view.cpp
    src/resident/domains/local_economy/views/production_history.cpp
    src/resident/domains/local_economy/production_management.cpp
    src/resident/domains/local_economy/production_configuration_projection.cpp
    src/resident/domains/local_economy/production_management_validation.cpp
    src/resident/domains/local_economy/membership_wages.cpp
    src/resident/domains/local_economy/local_economy_military.cpp
    src/resident/domains/local_economy/local_economy_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/market_history/economic_market_cause.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_economic_market_cause_cpp
    src/resident/domains/local_economy/market_history/economic_market_cause.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/supply/supply_policy_access.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_supply_policy_access_cpp
    src/resident/domains/local_economy/supply/supply_policy_access.cpp
    src/resident/domains/local_economy/supply/supply_policy_boundary.cpp
    src/resident/domains/local_economy/supply/supply_policy_codec.cpp
    src/resident/domains/local_economy/supply/supply_policy_ledger.cpp
    src/resident/domains/local_economy/supply/supply_policy_rules.cpp
    src/resident/domains/local_economy/supply/supply_policy_validation.cpp
    src/resident/domains/local_economy/supply/supply_policy_view.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/economic_native_lease.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_economic_native_lease_cpp
    src/resident/domains/local_economy/economic_native_lease.cpp
    src/resident/domains/local_economy/born_population_sources.cpp
    src/resident/domains/local_economy/household_service_lease.cpp
    src/resident/domains/local_economy/population_qualification_history.cpp
    src/resident/domains/local_economy/production/production_population_change.cpp
    src/resident/domains/local_economy/production/production_population_source.cpp
    src/resident/domains/local_economy/production/production_responsibility_change.cpp
    src/resident/domains/local_economy/production/production_input_source.cpp
    src/resident/domains/local_economy/production/facility_condition_mutation.cpp
    src/resident/domains/local_economy/production/production_input_lease.cpp
    src/resident/domains/local_economy/production/production_supply_input.cpp
    src/resident/domains/local_economy/production/production_capacity_input.cpp
    src/resident/domains/local_economy/production/production_adoption_source.cpp
    src/resident/domains/local_economy/production/production_labor_input.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/takeover/public_account_dependencies.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_public_account_dependencies_cpp
    src/resident/domains/local_economy/takeover/public_account_dependencies.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_fiscal_income_arithmetic_cpp
    src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.cpp
    src/resident/domains/local_economy/fiscal/fiscal_income_assessment.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_codec.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_fact.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_responsibility.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_rules.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_view.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/cohort_household_service.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_cohort_household_service_cpp
    src/resident/domains/local_economy/cohort_household_service.cpp
    src/resident/domains/local_economy/economic_calendar.cpp
    src/resident/domains/local_economy/economic_debt_needs.cpp
    src/resident/domains/local_economy/economic_exact_phase.cpp
    src/resident/domains/local_economy/economic_facility_codec.cpp
    src/resident/domains/local_economy/economic_facility_definition.cpp
    src/resident/domains/local_economy/economic_facility_provider.cpp
    src/resident/domains/local_economy/economic_facility_craft_sources.cpp
    src/resident/domains/local_economy/economic_facility_source_pins.cpp
    src/resident/domains/local_economy/economic_facility_sources.cpp
    src/resident/domains/local_economy/production/facility_construction_catalog.cpp
    src/resident/domains/local_economy/production/production_asset_lifecycle_codec.cpp
    src/resident/domains/local_economy/production/facility_construction_adoption.cpp
    src/resident/domains/local_economy/production/facility_construction_service.cpp
    src/resident/domains/local_economy/production/facility_construction_result_read.cpp
    src/resident/domains/local_economy/production/facility_construction_cost_capture.cpp
    src/resident/domains/local_economy/production/facility_construction_resource_effects.cpp
    src/resident/domains/local_economy/production/facility_construction_codec.cpp
    src/resident/domains/local_economy/production/production_asset_title_read.cpp
    src/resident/domains/local_economy/production/production_asset_transfer.cpp
    src/resident/domains/local_economy/production/production_asset_transfer_cash.cpp
    src/resident/domains/local_economy/production/production_asset_usage_read.cpp
    src/resident/domains/local_economy/production/production_asset_construction_authority.cpp
    src/resident/domains/local_economy/production/facility_construction_cost_writer.cpp
    src/resident/domains/local_economy/production/facility_construction_start.cpp
    src/resident/domains/local_economy/production/facility_construction_action_time.cpp
    src/resident/domains/local_economy/production/facility_construction_period_writer.cpp
    src/resident/domains/local_economy/economic_original_payments.cpp
    src/resident/domains/local_economy/economic_transport_codec.cpp
    src/resident/domains/local_economy/economic_transport_sources.cpp
    src/resident/domains/local_economy/economic_work_sources.cpp
    src/resident/domains/local_economy/economic_work_sources_codec.cpp
    src/resident/domains/local_economy/fiscal/budget_execution_codec.cpp
    src/resident/domains/local_economy/fiscal/budget_execution_contract.cpp
    src/resident/domains/local_economy/governance_work/economic_work_window.cpp
    src/resident/domains/local_economy/governance_work/governance_work_evidence.cpp
    src/resident/domains/local_economy/governance_work/governance_work_expense_codec.cpp
    src/resident/domains/local_economy/governance_work/governance_work_expense_contract.cpp
    src/resident/domains/local_economy/governance_work/governance_work_fee_policy.cpp
    src/resident/domains/local_economy/governance_work/governance_work_fee_terms_read.cpp
    src/resident/domains/local_economy/governance_work/governance_work_settlement.cpp
    src/resident/domains/local_economy/household_buyer_execution.cpp
    src/resident/domains/local_economy/household_cargo_handling.cpp
    src/resident/domains/local_economy/household_facility_sources.cpp
    src/resident/domains/local_economy/household_named_environment.cpp
    src/resident/domains/local_economy/household_purchase_cargo.cpp
    src/resident/domains/local_economy/household_purchase_codec.cpp
    src/resident/domains/local_economy/household_purchase_contract.cpp
    src/resident/domains/local_economy/household_purchase_demand.cpp
    src/resident/domains/local_economy/household_purchase_knowledge.cpp
    src/resident/domains/local_economy/household_retail_acceptance.cpp
    src/resident/domains/local_economy/household_service_runtime.cpp
    src/resident/domains/local_economy/household_transport_funding.cpp
    src/resident/domains/local_economy/market_price_rules.cpp
    src/resident/domains/local_economy/market_service_capacity.cpp
    src/resident/domains/local_economy/market_service_original.cpp
    src/resident/domains/local_economy/market_service_reconciliation.cpp
    src/resident/domains/local_economy/military_march_payment.cpp
    src/resident/domains/local_economy/named_household_service.cpp
    src/resident/domains/local_economy/population_manifest_history.cpp
    src/resident/domains/local_economy/population_revision_read.cpp
    src/resident/domains/local_economy/population_selection_source.cpp
    src/resident/domains/local_economy/population_transport.cpp
    src/resident/domains/local_economy/population_transport_codec.cpp
    src/resident/domains/local_economy/production/facility_condition_contract.cpp
    src/resident/domains/local_economy/public_market_staffing.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/household_mobile_custody.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_household_mobile_custody_cpp
    src/resident/domains/local_economy/household_mobile_custody.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/household_transport_registration.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_household_transport_registration_cpp
    # Fixed tactical measure and original owner continuation sources.
    src/resident/domains/local_economy/household_transport_registration.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/household_transport_read.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_household_transport_read_cpp
    src/resident/domains/local_economy/household_transport_read.cpp
    src/resident/domains/local_economy/production/facility_condition_history.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/economic_inventory_placement.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_economic_inventory_placement_cpp
    src/resident/domains/local_economy/economic_inventory_placement.cpp
    src/resident/domains/local_economy/household_cohort_environment.cpp
    src/resident/domains/local_economy/household_native_environment.cpp
    src/resident/domains/local_economy/production/production_supervision_target.cpp
)

# 生产输入连续登记段：src/resident/domains/local_economy/material_aid/material_aid_codec.cpp。
set(HANMO_RESIDENT_SOURCES_local_economy_material_aid_codec_cpp
    src/resident/domains/local_economy/material_aid/material_aid_codec.cpp
    src/resident/domains/local_economy/obligation_ref.cpp
    src/resident/domains/local_economy/population_change_time.cpp
    src/resident/domains/local_economy/population_count_revision.cpp
    src/resident/domains/local_economy/population_labor_history.cpp
    src/resident/domains/local_economy/population_training_history.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/obligations/transport_original_obligations.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_transport_original_obligations_hpp
    src/resident/domains/local_economy/obligations/transport_original_obligations.hpp
    src/resident/domains/local_economy/obligations/transport_original_obligations.cpp
    src/resident/domains/local_economy/obligations/transport_original_finance_detail.hpp
    src/resident/domains/local_economy/obligations/material_aid_original_obligations.hpp
    src/resident/domains/local_economy/obligations/material_aid_original_obligations.cpp
    src/resident/domains/local_economy/material_aid/material_aid_read.cpp
    src/resident/domains/local_economy/material_aid/material_aid_internal.hpp
    src/resident/domains/local_economy/material_aid/material_aid.hpp
    src/resident/domains/local_economy/material_aid/material_aid.cpp
    src/resident/domains/local_economy/household_transport_funding.hpp
    src/resident/domains/local_economy/economic_transport_sources.hpp
    src/resident/domains/local_economy/economic_original_payments.hpp
    src/resident/domains/local_economy/born_population_sources.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/service_finance_execution.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_service_finance_execution_hpp
    src/resident/domains/local_economy/service_finance_execution.hpp
    src/resident/domains/local_economy/service_finance_execution.cpp
    src/resident/domains/local_economy/obligations/original_service_payable.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_read.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_read.cpp
    src/resident/domains/local_economy/obligations/obligation_sources_pins.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_pins.cpp
    src/resident/domains/local_economy/obligations/obligation_sources_model.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_fields.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_codec.hpp
    src/resident/domains/local_economy/obligations/obligation_sources_codec.cpp
    src/resident/domains/local_economy/obligations/native_service_obligation_sources.hpp
    src/resident/domains/local_economy/obligations/native_service_obligation_sources.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/private_account_dependencies.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_private_account_dependencies_cpp
    src/resident/domains/local_economy/private_account_dependencies.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/private_account_history.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_private_account_history_cpp
    src/resident/domains/local_economy/private_account_history.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/private_account_dependencies.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_private_account_dependencies_hpp
    src/resident/domains/local_economy/private_account_dependencies.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/private_account_history.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_private_account_history_hpp
    src/resident/domains/local_economy/private_account_history.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_occupation_fiscal_need_profile_hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_debt_source.hpp
    src/resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/membership_wages.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_membership_wages_hpp
    src/resident/domains/local_economy/membership_wages.hpp
    src/resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp
    src/resident/domains/local_economy/wages/cohort_wage_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/takeover/execution_configurations.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_execution_configurations_cpp
    src/resident/domains/local_economy/takeover/execution_configurations.cpp
    src/resident/domains/local_economy/takeover/execution_configurations.hpp
    src/resident/domains/local_economy/takeover/execution_materials.cpp
    src/resident/domains/local_economy/takeover/execution_materials.hpp
    src/resident/domains/local_economy/takeover/execution_phase.cpp
    src/resident/domains/local_economy/takeover/execution_phase.hpp
    src/resident/domains/local_economy/takeover/execution_receipts.cpp
    src/resident/domains/local_economy/takeover/execution_receipts.hpp
    src/resident/domains/local_economy/takeover/execution_receipts_codec.cpp
    src/resident/domains/local_economy/takeover/public_account_dependencies.hpp
    src/resident/domains/local_economy/takeover/responsibility_execution.cpp
    src/resident/domains/local_economy/takeover/responsibility_execution.hpp
    src/resident/domains/local_economy/takeover/responsibility_history.hpp
    src/resident/domains/local_economy/takeover/responsibility_source.cpp
    src/resident/domains/local_economy/authored_obligation_read.hpp
    src/resident/domains/local_economy/takeover/responsibility_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/household/provision_management.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_provision_management_cpp
    src/resident/domains/local_economy/household/provision_management.cpp
    src/resident/domains/local_economy/household/provision_management_codec.cpp
    src/resident/domains/local_economy/household/provision_management_view.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions_codec.cpp
    src/resident/domains/local_economy/disruptions/production_disruptions_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/household/provision_management.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_provision_management_hpp
    src/resident/domains/local_economy/household/provision_management.hpp
    src/resident/domains/local_economy/household/provision_management_codec.hpp
    src/resident/domains/local_economy/household/provision_management_view.hpp
    src/resident/domains/local_economy/disruptions/production_disruptions.hpp
    src/resident/domains/local_economy/disruptions/production_disruptions_codec.hpp
    src/resident/domains/local_economy/disruptions/production_disruptions_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/fiscal/fiscal_settlement.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_fiscal_settlement_cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity.hpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_codec.hpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_internal.hpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_view.hpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_codec.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_rules.cpp
    src/resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_debt_source.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_debt_source.cpp
    src/resident/domains/local_economy/fiscal/fiscal_need_adapter.cpp
    src/resident/domains/local_economy/wages/cohort_wage_source.cpp
    src/resident/domains/local_economy/wages/cohort_wage_need_adapter.cpp
    src/resident/domains/local_economy/wages/cohort_wage_need_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_need_view.cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_codec.cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_view.cpp
    src/resident/domains/local_economy/relief/public_relief_rules.cpp
    src/resident/domains/local_economy/relief/public_relief_codec.cpp
    src/resident/domains/local_economy/relief/public_relief_catalog_links.cpp
    src/resident/domains/local_economy/local_economy_checkpoint.cpp
    src/resident/domains/local_economy/local_economy_codec.cpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement.hpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_codec.hpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp
    src/resident/domains/local_economy/relief/public_relief.hpp
    src/resident/domains/local_economy/relief/public_relief_rules.hpp
    src/resident/domains/local_economy/relief/public_relief_codec.hpp
    src/resident/domains/local_economy/relief/public_relief_catalog_links.hpp
    src/resident/domains/local_economy/local_economy_codec_detail.hpp
    src/resident/domains/local_economy/economic_payload_guard.hpp
    src/resident/domains/local_economy/local_economy_internal.hpp
    src/resident/domains/local_economy/household_provisioning.hpp
    src/resident/domains/local_economy/views/local_economy_view.hpp
    src/resident/domains/local_economy/views/production_history.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/local_economy_model.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_local_economy_model_hpp
    src/resident/domains/local_economy/local_economy_model.hpp
    src/resident/domains/local_economy/production_management.hpp
    src/resident/domains/local_economy/local_economy_military.hpp
    src/resident/domains/local_economy/local_economy_rules.cpp
    src/resident/domains/local_economy/household_provisioning.cpp
    src/resident/domains/local_economy/views/local_economy_view.cpp
    src/resident/domains/local_economy/views/production_history.cpp
    src/resident/domains/local_economy/production_management.cpp
    src/resident/domains/local_economy/production_configuration_projection.cpp
    src/resident/domains/local_economy/production_configuration_projection.hpp
    src/resident/domains/local_economy/production_management_validation.cpp
    src/resident/domains/local_economy/membership_wages.cpp
    src/resident/domains/local_economy/local_economy_military.cpp
    src/resident/domains/local_economy/local_economy_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/market_history/economic_market_cause.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_economic_market_cause_hpp
    src/resident/domains/local_economy/market_history/economic_market_cause.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/market_history/economic_market_cause.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_economic_market_cause_cpp
    src/resident/domains/local_economy/market_history/economic_market_cause.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/supply/supply_policy.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_supply_policy_hpp
    src/resident/domains/local_economy/supply/supply_policy.hpp
    src/resident/domains/local_economy/supply/supply_policy_boundary.hpp
    src/resident/domains/local_economy/supply/supply_policy_codec.hpp
    src/resident/domains/local_economy/supply/supply_policy_contract.hpp
    src/resident/domains/local_economy/supply/supply_policy_internal.hpp
    src/resident/domains/local_economy/supply/supply_policy_profile.hpp
    src/resident/domains/local_economy/supply/supply_policy_rules.hpp
    src/resident/domains/local_economy/supply/supply_policy_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/supply/supply_policy_access.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_supply_policy_access_cpp
    src/resident/domains/local_economy/supply/supply_policy_access.cpp
    src/resident/domains/local_economy/supply/supply_policy_boundary.cpp
    src/resident/domains/local_economy/supply/supply_policy_codec.cpp
    src/resident/domains/local_economy/supply/supply_policy_ledger.cpp
    src/resident/domains/local_economy/supply/supply_policy_rules.cpp
    src/resident/domains/local_economy/supply/supply_policy_validation.cpp
    src/resident/domains/local_economy/supply/supply_policy_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/economic_native_lease.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_economic_native_lease_cpp
    src/resident/domains/local_economy/economic_native_lease.cpp
    src/resident/domains/local_economy/born_population_sources.cpp
    src/resident/domains/local_economy/household_service_lease.cpp
    src/resident/domains/local_economy/population_qualification_history.cpp
    src/resident/domains/local_economy/production/production_population_change.cpp
    src/resident/domains/local_economy/production/production_population_source.cpp
    src/resident/domains/local_economy/production/production_responsibility_change.cpp
    src/resident/domains/local_economy/production/production_input_source.cpp
    src/resident/domains/local_economy/production/facility_condition_mutation.cpp
    src/resident/domains/local_economy/production/production_input_lease.cpp
    src/resident/domains/local_economy/production/production_supply_input.cpp
    src/resident/domains/local_economy/production/production_capacity_input.cpp
    src/resident/domains/local_economy/production/production_adoption_source.cpp
    src/resident/domains/local_economy/production/production_labor_input.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/takeover/public_account_dependencies.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_public_account_dependencies_cpp
    src/resident/domains/local_economy/takeover/public_account_dependencies.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_fiscal_income_arithmetic_cpp
    src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.cpp
    src/resident/domains/local_economy/fiscal/fiscal_income_assessment.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_codec.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_fact.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_responsibility.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_rules.cpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.hpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_fiscal_income_arithmetic_hpp
    src/resident/domains/local_economy/fiscal/fiscal_income_arithmetic.hpp
    src/resident/domains/local_economy/fiscal/fiscal_income_assessment.hpp
    src/resident/domains/local_economy/fiscal/fiscal_settlement_internal.hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_codec.hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_responsibility.hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp
    src/resident/domains/local_economy/fiscal/occupation_fiscal_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/cohort_household_service.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_cohort_household_service_cpp
    src/resident/domains/local_economy/cohort_household_service.cpp
    src/resident/domains/local_economy/economic_calendar.cpp
    src/resident/domains/local_economy/economic_debt_needs.cpp
    src/resident/domains/local_economy/economic_exact_phase.cpp
    src/resident/domains/local_economy/economic_facility_codec.cpp
    src/resident/domains/local_economy/economic_facility_definition.cpp
    src/resident/domains/local_economy/economic_facility_provider.cpp
    src/resident/domains/local_economy/economic_facility_craft_sources.cpp
    src/resident/domains/local_economy/economic_facility_craft_sources.hpp
    src/resident/domains/local_economy/economic_facility_initial_usage.hpp
    src/resident/domains/local_economy/economic_facility_source_pins.cpp
    src/resident/domains/local_economy/economic_facility_sources.cpp
    src/resident/domains/local_economy/production/facility_construction_catalog.cpp
    src/resident/domains/local_economy/production/production_asset_lifecycle_codec.cpp
    src/resident/domains/local_economy/production/facility_construction_adoption.cpp
    src/resident/domains/local_economy/production/facility_construction_service.cpp
    src/resident/domains/local_economy/production/facility_construction_result_read.cpp
    src/resident/domains/local_economy/production/facility_construction_cost_capture.cpp
    src/resident/domains/local_economy/production/facility_construction_resource_effects.cpp
    src/resident/domains/local_economy/production/facility_construction_codec.cpp
    src/resident/domains/local_economy/production/production_asset_title_read.cpp
    src/resident/domains/local_economy/production/production_asset_transfer.cpp
    src/resident/domains/local_economy/production/production_asset_transfer_cash.cpp
    src/resident/domains/local_economy/production/production_asset_usage_read.cpp
    src/resident/domains/local_economy/production/production_asset_construction_authority.cpp
    src/resident/domains/local_economy/production/facility_construction_cost_writer.cpp
    src/resident/domains/local_economy/production/facility_construction_start.cpp
    src/resident/domains/local_economy/production/facility_construction_action_time.cpp
    src/resident/domains/local_economy/production/facility_construction_period_writer.cpp
    src/resident/domains/local_economy/production/production_asset_lifecycle.hpp
    src/resident/domains/local_economy/production/production_asset_title_read.hpp
    src/resident/domains/local_economy/production/production_asset_transfer.hpp
    src/resident/domains/local_economy/production/production_asset_transfer_cash.hpp
    src/resident/domains/local_economy/production/production_asset_usage_read.hpp
    src/resident/domains/local_economy/production/production_asset_construction_authority.hpp
    src/resident/domains/local_economy/production/production_asset_construction_candidate.hpp
    src/resident/domains/local_economy/production/facility_construction_cost_writer.hpp
    src/resident/domains/local_economy/production/facility_construction_start.hpp
    src/resident/domains/local_economy/production/facility_construction_service_original.hpp
    src/resident/domains/local_economy/production/facility_construction_action_time.hpp
    src/resident/domains/local_economy/production/facility_construction_period_writer.hpp
    src/resident/domains/local_economy/production/facility_construction_model.hpp
    src/resident/domains/local_economy/production/facility_construction_catalog.hpp
    src/resident/domains/local_economy/production/facility_construction_adoption.hpp
    src/resident/domains/local_economy/production/facility_construction_service.hpp
    src/resident/domains/local_economy/production/facility_construction_result_read.hpp
    src/resident/domains/local_economy/production/facility_construction_cost_capture.hpp
    src/resident/domains/local_economy/production/facility_construction_resource_effects.hpp
    src/resident/domains/local_economy/production/facility_construction_codec.hpp
    src/resident/domains/local_economy/production/facility_construction_fields.hpp
    src/resident/domains/local_economy/production/production_asset_lifecycle_codec.hpp
    src/resident/domains/local_economy/production/production_asset_lifecycle_fields.hpp
    src/resident/domains/local_economy/economic_inventory_placement.cpp
    src/resident/domains/local_economy/economic_original_payments.cpp
    src/resident/domains/local_economy/economic_transport_codec.cpp
    src/resident/domains/local_economy/economic_transport_sources.cpp
    src/resident/domains/local_economy/economic_work_sources.cpp
    src/resident/domains/local_economy/economic_work_sources_codec.cpp
    src/resident/domains/local_economy/fiscal/budget_execution_codec.cpp
    src/resident/domains/local_economy/fiscal/budget_execution_contract.cpp
    src/resident/domains/local_economy/governance_work/economic_work_window.cpp
    src/resident/domains/local_economy/governance_work/governance_work_evidence.cpp
    src/resident/domains/local_economy/governance_work/governance_work_expense_codec.cpp
    src/resident/domains/local_economy/governance_work/governance_work_expense_contract.cpp
    src/resident/domains/local_economy/governance_work/governance_work_fee_policy.cpp
    src/resident/domains/local_economy/governance_work/governance_work_fee_terms_read.cpp
    src/resident/domains/local_economy/governance_work/governance_work_settlement.cpp
    src/resident/domains/local_economy/household_buyer_execution.cpp
    src/resident/domains/local_economy/household_cargo_handling.cpp
    src/resident/domains/local_economy/household_cohort_environment.cpp
    src/resident/domains/local_economy/household_facility_sources.cpp
    src/resident/domains/local_economy/household_mobile_custody.cpp
    src/resident/domains/local_economy/household_named_environment.cpp
    src/resident/domains/local_economy/household_native_environment.cpp
    src/resident/domains/local_economy/household_purchase_cargo.cpp
    src/resident/domains/local_economy/household_purchase_codec.cpp
    src/resident/domains/local_economy/household_purchase_contract.cpp
    src/resident/domains/local_economy/household_purchase_demand.cpp
    src/resident/domains/local_economy/household_purchase_knowledge.cpp
    src/resident/domains/local_economy/household_retail_acceptance.cpp
    src/resident/domains/local_economy/household_service_runtime.cpp
    src/resident/domains/local_economy/household_transport_funding.cpp
    src/resident/domains/local_economy/household_transport_read.cpp
    src/resident/domains/local_economy/household_transport_registration.cpp
    src/resident/domains/local_economy/market_price_rules.cpp
    src/resident/domains/local_economy/market_service_capacity.cpp
    src/resident/domains/local_economy/market_service_original.cpp
    src/resident/domains/local_economy/market_service_reconciliation.cpp
    src/resident/domains/local_economy/military_march_payment.cpp
    src/resident/domains/local_economy/named_household_service.cpp
    src/resident/domains/local_economy/population_manifest_history.cpp
    src/resident/domains/local_economy/population_revision_read.cpp
    src/resident/domains/local_economy/population_selection_source.cpp
    src/resident/domains/local_economy/population_transport.cpp
    src/resident/domains/local_economy/population_transport_codec.cpp
    src/resident/domains/local_economy/production/facility_condition_contract.cpp
    src/resident/domains/local_economy/production/facility_condition_history.cpp
    src/resident/domains/local_economy/production/production_supervision_target.cpp
    src/resident/domains/local_economy/public_market_staffing.cpp
)

# 生产身份输入连续登记段：src/resident/domains/local_economy/material_aid/material_aid_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_local_economy_material_aid_codec_cpp
    src/resident/domains/local_economy/material_aid/material_aid_codec.cpp
    src/resident/domains/local_economy/obligation_ref.cpp
    src/resident/domains/local_economy/population_change_time.cpp
    src/resident/domains/local_economy/population_count_revision.cpp
    src/resident/domains/local_economy/population_labor_history.cpp
    src/resident/domains/local_economy/population_training_history.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/local_economy/source_owned_fields/fiscal_obligations.inc
    src/resident/domains/local_economy/source_owned_fields/population_household.inc
    src/resident/domains/local_economy/source_owned_fields/production_facilities.inc
    src/resident/domains/local_economy/source_owned_fields/work_expenses_supply.inc
    src/resident/domains/local_economy/world_codec/catalog_occupation_fiscal.inc
    src/resident/domains/local_economy/world_codec/catalog_production_household.inc
    src/resident/domains/local_economy/world_codec/catalog_relief_settlement.inc
    src/resident/domains/local_economy/world_codec/catalog_supply_policies.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_economic_facility_capacity_sources.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_economic_obligation_sources.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_economic_supply_policies.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_economic_takeover_execution.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_fiscal_taxpayer_continuity.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_household_provision_management.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_household_purchases.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_local_economy.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_occupation_fiscal_settlement.inc
    src/resident/domains/local_economy/world_codec/checkpoint_read_production_cycles.inc
    src/resident/domains/local_economy/world_codec/checkpoint_write_facility_roots.inc
    src/resident/domains/local_economy/world_codec/checkpoint_write_obligations.inc
    src/resident/domains/local_economy/world_codec/checkpoint_write_original.inc
    src/resident/domains/local_economy/world_codec/checkpoint_write_production_cycles.inc
    src/resident/domains/local_economy/world_codec/export_facility_fact_payloads.inc
    src/resident/domains/local_economy/world_codec/export_facility_state.inc
    src/resident/domains/local_economy/world_codec/export_original_obligations.inc
    src/resident/domains/local_economy/world_codec/export_production_cycles.inc
    src/resident/domains/local_economy/world_codec/seed_dynamic_economy.inc
    src/resident/domains/local_economy/world_codec/seed_facility_roots.inc
    src/resident/domains/local_economy/world_codec/seed_governance_programs.inc
    src/resident/domains/local_economy/world_codec/seed_household_fiscal.inc
    src/resident/domains/local_economy/world_codec/seed_household_purchases.inc
    src/resident/domains/local_economy/world_codec/seed_obligation_selection.inc
    src/resident/domains/local_economy/world_codec/seed_obligation_sources.inc
    src/resident/domains/local_economy/world_codec/seed_production_cycles.inc
    src/resident/domains/local_economy/world_codec/seed_public_relief.inc
    src/resident/domains/local_economy/world_codec/seed_supply_policies.inc
    src/resident/domains/local_economy/world_codec/selection_management.inc
    src/resident/domains/local_economy/world_codec/selection_production.inc
)
