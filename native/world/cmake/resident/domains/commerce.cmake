# 商业与服务交易：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/commerce/repair_service/repair_service_settlement.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_repair_service_settlement_cpp
    src/resident/domains/commerce/repair_service/repair_service_settlement.cpp
    src/resident/domains/commerce/repair_service/repair_service_rules.cpp
    src/resident/domains/commerce/repair_service/repair_service_resource.cpp
    src/resident/domains/commerce/repair_service/repair_service_read.cpp
    src/resident/domains/commerce/repair_service/repair_service_private_finance.cpp
    src/resident/domains/commerce/repair_service/repair_service_native.cpp
    src/resident/domains/commerce/repair_service/repair_service_money.cpp
    src/resident/domains/commerce/repair_service/repair_service_item_transfer.cpp
    src/resident/domains/commerce/repair_service/repair_service_financial_sources.cpp
    src/resident/domains/commerce/repair_service/repair_service_execution.cpp
    src/resident/domains/commerce/repair_service/repair_service_contract.cpp
    src/resident/domains/commerce/repair_service/repair_service_command_codec.cpp
    src/resident/domains/commerce/repair_service/repair_service_command.cpp
    src/resident/domains/commerce/repair_service/repair_service_codec.cpp
    src/resident/domains/commerce/repair_service/repair_service_agreement_source.cpp
    src/resident/domains/commerce/repair_service/repair_service_agreement_read.cpp
    src/resident/domains/commerce/repair_service/repair_service_acceptance.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_contact_source.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_travel_contact_source_cpp
    src/resident/domains/commerce/freight/private_shop_travel_contact_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_encounter_leave_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_replan_source.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_control_source.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_travel_control_source_cpp
    src/resident/domains/commerce/freight/private_shop_travel_control_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_departure_source.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_payment_source.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_travel_payment_source_cpp
    src/resident/domains/commerce/freight/private_shop_travel_payment_source.cpp
    src/resident/domains/commerce/freight/private_shop_freight_recipe_projection.cpp
    src/resident/domains/commerce/freight/private_shop_freight_purchase_source.cpp
    src/resident/domains/commerce/freight/private_shop_freight_estate_source.cpp
    src/resident/domains/commerce/freight/private_shop_stock_movement_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_rescue_loss.cpp
    src/resident/domains/commerce/freight/private_shop_freight_resource_scope.cpp
    src/resident/domains/commerce/freight/private_shop_travel_termination_source.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/command_results/private_shop_command_result_sources.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_command_result_sources_cpp
    src/resident/domains/commerce/command_results/private_shop_command_result_sources.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_codec.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_commit.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_validation.cpp
    src/resident/domains/commerce/command_results/private_shop_command_result_player.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_daily_rent_player_history_cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_authority.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_business.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_calendar.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_financial.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_sources.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_view.cpp
    src/resident/domains/commerce/daily_rent/private_shop_rent_period.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/liquidation/private_shop_liquidation_access.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_liquidation_access_cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_access.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_authority.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_boundary.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_cached_receipt.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_closing.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_codec.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_rules.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_view.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/private_shop_accounting.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_accounting_cpp
    src/resident/domains/commerce/private_shop_accounting.cpp
    src/resident/domains/commerce/private_shop_asset_responsibilities.cpp
    src/resident/domains/commerce/private_shop_economy_sources.cpp
    src/resident/domains/commerce/private_shop_assets.cpp
    src/resident/domains/commerce/private_shop_boundary.cpp
    src/resident/domains/commerce/private_shop_business_view.cpp
    src/resident/domains/commerce/private_shop_cached_receipt.cpp
    src/resident/domains/commerce/private_shop_closing.cpp
    src/resident/domains/commerce/private_shop_codec.cpp
    src/resident/domains/commerce/private_shop_eligibility.cpp
    src/resident/domains/commerce/private_shop_management.cpp
    src/resident/domains/commerce/private_shop_opening.cpp
    src/resident/domains/commerce/private_shop_operation.cpp
    src/resident/domains/commerce/private_shop_operation_validation.cpp
    src/resident/domains/commerce/private_shop_order_settlement.cpp
    src/resident/domains/commerce/private_shop_orders.cpp
    src/resident/domains/commerce/private_shop_order_contact.cpp
    src/resident/domains/commerce/private_shop_payload.cpp
    src/resident/domains/commerce/private_shop_read.cpp
    src/resident/domains/commerce/private_shop_restock.cpp
    src/resident/domains/commerce/private_shop_staff.cpp
    src/resident/domains/commerce/private_shop_staff_exclusion.cpp
    src/resident/domains/commerce/private_shop_staff_read.cpp
    src/resident/domains/commerce/private_shop_staff_service.cpp
    src/resident/domains/commerce/private_shop_trade_pins.cpp
    src/resident/domains/commerce/private_shop_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/private_shop_market_service.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_market_service_cpp
    src/resident/domains/commerce/private_shop_market_service.cpp
    src/resident/domains/commerce/private_shop_staff_exact.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/freight/private_shop_freight_capture_projection.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_freight_capture_projection_cpp
    src/resident/domains/commerce/freight/private_shop_freight_capture_projection.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/household_retail.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_household_retail_cpp
    src/resident/domains/commerce/household_retail.cpp
    src/resident/domains/commerce/private_shop_absence.cpp
    src/resident/domains/commerce/verified_trade_recipe.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/service_agreement.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_service_agreement_cpp
    src/resident/domains/commerce/service_agreement.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/private_shop_exact_service.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_shop_exact_service_cpp
    src/resident/domains/commerce/private_shop_exact_service.cpp
    src/resident/domains/commerce/private_shop_capability_codec.cpp
    src/resident/domains/commerce/private_shop_capability.cpp
    src/resident/domains/commerce/private_shop_terminal_sources.cpp
    src/resident/domains/commerce/private_shop_labor_sources.cpp
    src/resident/domains/commerce/private_shop_operation_sources_codec.cpp
    src/resident/domains/commerce/private_shop_operation_sources.cpp
    src/resident/domains/commerce/private_shop_action_time.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/private_wallet_title.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_private_wallet_title_cpp
    src/resident/domains/commerce/private_wallet_title.cpp
    src/resident/domains/commerce/self_transport_funding.cpp
    src/resident/domains/commerce/self_transport_funding_codec.cpp
    src/resident/domains/commerce/personal_transport_funding.cpp
)

# 生产输入连续登记段：src/resident/domains/commerce/repair_service/repair_item_command.cpp。
set(HANMO_RESIDENT_SOURCES_commerce_repair_item_command_cpp
    src/resident/domains/commerce/repair_service/repair_item_command.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical_memory.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical_read.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/repair_service/repair_service_settlement.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_repair_service_settlement_cpp
    src/resident/domains/commerce/repair_service/repair_service_settlement.cpp
    src/resident/domains/commerce/repair_service/repair_service_rules.hpp
    src/resident/domains/commerce/repair_service/repair_service_rules.cpp
    src/resident/domains/commerce/repair_service/repair_service_resource.cpp
    src/resident/domains/commerce/repair_service/repair_service_read.cpp
    src/resident/domains/commerce/repair_service/repair_service_private_finance.hpp
    src/resident/domains/commerce/repair_service/repair_service_private_finance.cpp
    src/resident/domains/commerce/repair_service/repair_service_native.hpp
    src/resident/domains/commerce/repair_service/repair_service_native.cpp
    src/resident/domains/commerce/repair_service/repair_service_money.cpp
    src/resident/domains/commerce/repair_service/repair_service_model.hpp
    src/resident/domains/commerce/repair_service/repair_service_item_transfer.hpp
    src/resident/domains/commerce/repair_service/repair_service_item_transfer.cpp
    src/resident/domains/commerce/repair_service/repair_service_internal.hpp
    src/resident/domains/commerce/repair_service/repair_service_financial_sources.hpp
    src/resident/domains/commerce/repair_service/repair_service_financial_sources.cpp
    src/resident/domains/commerce/repair_service/repair_service_fields.hpp
    src/resident/domains/commerce/repair_service/repair_service_execution.cpp
    src/resident/domains/commerce/repair_service/repair_service_contract.cpp
    src/resident/domains/commerce/repair_service/repair_service_command_model.hpp
    src/resident/domains/commerce/repair_service/repair_service_command_codec.cpp
    src/resident/domains/commerce/repair_service/repair_service_command.hpp
    src/resident/domains/commerce/repair_service/repair_service_command.cpp
    src/resident/domains/commerce/repair_service/repair_service_codec_archive.hpp
    src/resident/domains/commerce/repair_service/repair_service_codec.hpp
    src/resident/domains/commerce/repair_service/repair_service_codec.cpp
    src/resident/domains/commerce/repair_service/repair_service_agreement_source.cpp
    src/resident/domains/commerce/repair_service/repair_service_agreement_read.cpp
    src/resident/domains/commerce/repair_service/repair_service_agreement_internal.hpp
    src/resident/domains/commerce/repair_service/repair_service_agreement.hpp
    src/resident/domains/commerce/repair_service/repair_service_acceptance.cpp
    src/resident/domains/commerce/repair_service/repair_service.hpp
    src/resident/domains/commerce/business_financing/investment_contract_ref.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_contact_source.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_travel_contact_source_cpp
    src/resident/domains/commerce/freight/private_shop_travel_contact_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_contact_source.hpp
    src/resident/domains/commerce/freight/private_shop_travel_encounter_leave_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_encounter_leave_source.hpp
    src/resident/domains/commerce/freight/private_shop_travel_replan_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_replan_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_control_source.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_travel_control_source_cpp
    src/resident/domains/commerce/freight/private_shop_travel_control_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_departure_source.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_control_source.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_travel_control_source_hpp
    src/resident/domains/commerce/freight/private_shop_travel_control_source.hpp
    src/resident/domains/commerce/freight/private_shop_travel_departure_source.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_travel_expense_contract.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_travel_expense_contract_hpp
    src/resident/domains/commerce/freight/private_shop_travel_expense_contract.hpp
    src/resident/domains/commerce/freight/private_shop_travel_payment_source.hpp
    src/resident/domains/commerce/freight/private_shop_freight_recipe_projection.hpp
    src/resident/domains/commerce/freight/private_shop_freight_purchase_source.hpp
    src/resident/domains/commerce/freight/private_shop_freight_estate_source.hpp
    src/resident/domains/commerce/freight/private_shop_stock_movement_source.hpp
    src/resident/domains/commerce/freight/private_shop_travel_rescue_loss.hpp
    src/resident/domains/commerce/freight/private_shop_freight_resource_scope.hpp
    src/resident/domains/commerce/freight/private_shop_travel_payment_source.cpp
    src/resident/domains/commerce/freight/private_shop_freight_recipe_projection.cpp
    src/resident/domains/commerce/freight/private_shop_freight_purchase_source.cpp
    src/resident/domains/commerce/freight/private_shop_freight_estate_source.cpp
    src/resident/domains/commerce/freight/private_shop_stock_movement_source.cpp
    src/resident/domains/commerce/freight/private_shop_travel_rescue_loss.cpp
    src/resident/domains/commerce/freight/private_shop_freight_resource_scope.cpp
    src/resident/domains/commerce/freight/private_shop_travel_termination_source.hpp
    src/resident/domains/commerce/freight/private_shop_travel_termination_source.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/command_results/private_shop_command_result_sources.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_command_result_sources_cpp
    src/resident/domains/commerce/command_results/private_shop_command_result_sources.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_codec.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_commit.cpp
    src/resident/domains/commerce/command_results/private_shop_command_results_validation.cpp
    src/resident/domains/commerce/command_results/private_shop_command_result_player.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/command_results/private_shop_command_results.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_command_results_hpp
    src/resident/domains/commerce/command_results/private_shop_command_results.hpp
    src/resident/domains/commerce/command_results/private_shop_command_result_sources.hpp
    src/resident/domains/commerce/command_results/private_shop_command_results_codec.hpp
    src/resident/domains/commerce/command_results/private_shop_command_result_player.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_daily_rent_player_history_cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_player_history.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_authority.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_business.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_calendar.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_financial.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_sources.cpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_view.cpp
    src/resident/domains/commerce/daily_rent/private_shop_rent_period.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/daily_rent/private_shop_daily_rent.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_daily_rent_hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_business.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_codec.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_financial.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_internal.hpp
    src/resident/domains/commerce/daily_rent/private_shop_daily_rent_view.hpp
    src/resident/domains/commerce/daily_rent/private_shop_rent_period.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/liquidation/private_shop_liquidation.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_liquidation_hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_boundary.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_cached_receipt.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_codec.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_contract.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_detail.hpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_view.hpp
    src/resident/domains/commerce/private_shop_closing_assets.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/liquidation/private_shop_liquidation_access.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_liquidation_access_cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_access.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_authority.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_boundary.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_cached_receipt.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_closing.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_codec.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_rules.cpp
    src/resident/domains/commerce/liquidation/private_shop_liquidation_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_shop.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_hpp
    src/resident/domains/commerce/private_shop.hpp
    src/resident/domains/commerce/private_shop_accounting.hpp
    src/resident/domains/commerce/private_shop_asset_responsibilities.hpp
    src/resident/domains/commerce/private_shop_assets.hpp
    src/resident/domains/commerce/private_shop_boundary.hpp
    src/resident/domains/commerce/private_shop_business_view.hpp
    src/resident/domains/commerce/private_shop_cached_receipt.hpp
    src/resident/domains/commerce/private_shop_closing.hpp
    src/resident/domains/commerce/private_shop_codec.hpp
    src/resident/domains/commerce/private_shop_contract.hpp
    src/resident/domains/commerce/private_shop_eligibility.hpp
    src/resident/domains/commerce/private_shop_market_contract.hpp
    src/resident/domains/commerce/private_shop_operation.hpp
    src/resident/domains/commerce/private_shop_order_contract.hpp
    src/resident/domains/commerce/private_shop_orders.hpp
    src/resident/domains/commerce/private_shop_order_contact.hpp
    src/resident/domains/commerce/private_shop_payload.hpp
    src/resident/domains/commerce/private_shop_read.hpp
    src/resident/domains/commerce/private_shop_restock.hpp
    src/resident/domains/commerce/private_shop_staff.hpp
    src/resident/domains/commerce/private_shop_staff_contract.hpp
    src/resident/domains/commerce/private_shop_staff_exclusion.hpp
    src/resident/domains/commerce/private_shop_trade_pins.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_shop_accounting.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_accounting_cpp
    src/resident/domains/commerce/private_shop_accounting.cpp
    src/resident/domains/commerce/private_shop_asset_responsibilities.cpp
    src/resident/domains/commerce/private_shop_economy_sources.cpp
    src/resident/domains/commerce/private_shop_assets.cpp
    src/resident/domains/commerce/private_shop_boundary.cpp
    src/resident/domains/commerce/private_shop_business_view.cpp
    src/resident/domains/commerce/private_shop_cached_receipt.cpp
    src/resident/domains/commerce/private_shop_closing.cpp
    src/resident/domains/commerce/private_shop_codec.cpp
    src/resident/domains/commerce/private_shop_eligibility.cpp
    src/resident/domains/commerce/private_shop_management.cpp
    src/resident/domains/commerce/private_shop_opening.cpp
    src/resident/domains/commerce/private_shop_operation.cpp
    src/resident/domains/commerce/private_shop_operation_validation.cpp
    src/resident/domains/commerce/private_shop_order_settlement.cpp
    src/resident/domains/commerce/private_shop_orders.cpp
    src/resident/domains/commerce/private_shop_order_contact.cpp
    src/resident/domains/commerce/private_shop_payload.cpp
    src/resident/domains/commerce/private_shop_read.cpp
    src/resident/domains/commerce/private_shop_restock.cpp
    src/resident/domains/commerce/private_shop_staff.cpp
    src/resident/domains/commerce/private_shop_staff_exclusion.cpp
    src/resident/domains/commerce/private_shop_staff_read.cpp
    src/resident/domains/commerce/private_shop_staff_service.cpp
    src/resident/domains/commerce/private_shop_trade_pins.cpp
    src/resident/domains/commerce/private_shop_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_shop_market_service.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_market_service_cpp
    src/resident/domains/commerce/private_shop_market_service.cpp
    src/resident/domains/commerce/private_shop_staff_exact.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_freight_capture_projection.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_freight_capture_projection_cpp
    src/resident/domains/commerce/freight/private_shop_freight_capture_projection.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/freight/private_shop_freight_capture_projection.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_freight_capture_projection_hpp
    src/resident/domains/commerce/freight/private_shop_freight_capture_projection.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/household_retail.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_household_retail_cpp
    src/resident/domains/commerce/household_retail.cpp
    src/resident/domains/commerce/private_shop_absence.cpp
    src/resident/domains/commerce/private_shop_absence.hpp
    src/resident/domains/commerce/verified_trade_recipe.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/service_agreement.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_service_agreement_hpp
    src/resident/domains/commerce/service_agreement.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_wallet_title.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_wallet_title_cpp
    src/resident/domains/commerce/private_wallet_title.cpp
    src/resident/domains/commerce/self_transport_funding.cpp
    src/resident/domains/commerce/self_transport_funding_codec.cpp
    src/resident/domains/commerce/personal_transport_funding.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_wallet_title.hpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_wallet_title_hpp
    src/resident/domains/commerce/private_wallet_title.hpp
    src/resident/domains/commerce/self_transport_funding.hpp
    src/resident/domains/commerce/self_transport_funding_fields.hpp
    src/resident/domains/commerce/personal_transport_funding.hpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/private_shop_exact_service.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_private_shop_exact_service_cpp
    src/resident/domains/commerce/private_shop_exact_service.cpp
    src/resident/domains/commerce/private_shop_capability_codec.cpp
    src/resident/domains/commerce/private_shop_capability.cpp
    src/resident/domains/commerce/private_shop_terminal_sources.cpp
    src/resident/domains/commerce/private_shop_labor_sources.cpp
    src/resident/domains/commerce/private_shop_operation_sources_codec.cpp
    src/resident/domains/commerce/private_shop_operation_sources.cpp
    src/resident/domains/commerce/private_shop_action_time.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/service_agreement.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_service_agreement_cpp
    src/resident/domains/commerce/service_agreement.cpp
)

# 生产身份输入连续登记段：src/resident/domains/commerce/repair_service/repair_item_command.cpp。
set(HANMO_RESIDENT_IDENTITY_commerce_repair_item_command_cpp
    src/resident/domains/commerce/repair_service/repair_item_command.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical_memory.cpp
    src/resident/domains/commerce/repair_service/repair_service_physical_read.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/commerce/source_owned_fields/domain_fields.inc
    src/resident/domains/commerce/world_codec/catalog_shop_legal_profiles.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_household_retail_settlements.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_private_shop_daily_rent_module.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_private_shop_liquidations.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_private_shop_operation_sources.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_private_shops.inc
    src/resident/domains/commerce/world_codec/checkpoint_read_private_transport_funding.inc
    src/resident/domains/commerce/world_codec/checkpoint_write_shop_operations.inc
    src/resident/domains/commerce/world_codec/checkpoint_write_transport_funding.inc
    src/resident/domains/commerce/world_codec/export_shop_operations.inc
    src/resident/domains/commerce/world_codec/export_transport_funding.inc
    src/resident/domains/commerce/world_codec/fact_kind_selection.inc
    src/resident/domains/commerce/world_codec/seed_command_results.inc
    src/resident/domains/commerce/world_codec/seed_liquidations.inc
    src/resident/domains/commerce/world_codec/seed_rent_shops.inc
    src/resident/domains/commerce/world_codec/seed_shop_operations.inc
    src/resident/domains/commerce/world_codec/seed_transport_funding.inc
    src/resident/domains/commerce/world_codec/seed_validate_shop_payloads.inc
    src/resident/domains/commerce/world_codec/shop_estate_fact_projection.inc
    src/resident/domains/commerce/world_codec/shop_market_fact_projection.inc
)
