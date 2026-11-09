# 任务与履约：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/tasks/task_actor_pending_responsibilities.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_actor_pending_responsibilities_cpp
    src/resident/domains/tasks/task_actor_pending_responsibilities.cpp
    src/resident/domains/tasks/task_inheritance_history.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_rules.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_codec.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_validation.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_cash_sources.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_view.cpp
    src/resident/domains/tasks/extensions/task_extension_catalog.cpp
    src/resident/domains/tasks/extensions/task_extension_codec.cpp
    src/resident/domains/tasks/extensions/task_extension_rules.cpp
    src/resident/domains/tasks/extensions/task_extension_validation.cpp
    src/resident/domains/tasks/extensions/task_extension_boundary.cpp
    src/resident/domains/tasks/extensions/task_extension_view.cpp
    src/resident/domains/tasks/extensions/task_extension_world.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_authored_continuation.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_authored_continuation_cpp
    src/resident/domains/tasks/task_authored_continuation.cpp
    src/resident/domains/tasks/task_responsibility_continuation.cpp
    src/resident/domains/tasks/task_responsibility_director.cpp
    src/resident/domains/tasks/task_responsibility_director_world.cpp
    src/resident/domains/tasks/task_authored_continuation_catalog.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_public_account_codec.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_public_account_codec_cpp
    src/resident/domains/tasks/task_public_account_codec.cpp
    src/resident/domains/tasks/task_public_account_context.cpp
    src/resident/domains/tasks/task_public_account_dependencies.cpp
    src/resident/domains/tasks/task_public_account_execution.cpp
    src/resident/domains/tasks/task_public_account_payment.cpp
    src/resident/domains/tasks/task_public_account_reservations.cpp
    src/resident/domains/tasks/task_public_account_view.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_dynamic_codec.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_dynamic_codec_cpp
    src/resident/domains/tasks/task_dynamic_codec.cpp
    src/resident/domains/tasks/task_dynamic_rules.cpp
    src/resident/domains/tasks/objectives/objective_result_committed.cpp
    src/resident/domains/tasks/task_dynamic_cargo.cpp
    src/resident/domains/tasks/task_dynamic_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_order_estate.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_shop_order_estate_cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_boundary.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_codec.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_receipts.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_shop_asset_responsibilities_cpp
    src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_acceptance.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_actions.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_boundary.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_business.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_receipt.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_source.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_codec.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_codec.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_rules.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_validation.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_receipts.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_access.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_profile.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_control.cpp
    src/resident/domains/tasks/message_returns/task_message_return_boundary.cpp
    src/resident/domains/tasks/message_returns/task_message_return_catalog.cpp
    src/resident/domains/tasks/message_returns/task_message_return_codec.cpp
    src/resident/domains/tasks/message_returns/task_message_return_collection.cpp
    src/resident/domains/tasks/message_returns/task_message_return_control.cpp
    src/resident/domains/tasks/message_returns/task_message_return_delivery.cpp
    src/resident/domains/tasks/message_returns/task_message_return_finish.cpp
    src/resident/domains/tasks/message_returns/task_message_return_profile.cpp
    src/resident/domains/tasks/message_returns/task_message_return_progress.cpp
    src/resident/domains/tasks/message_returns/task_message_return_receipts.cpp
    src/resident/domains/tasks/message_returns/task_message_return_report_start.cpp
    src/resident/domains/tasks/message_returns/task_message_return_report_validation.cpp
    src/resident/domains/tasks/message_returns/task_message_return_services.cpp
    src/resident/domains/tasks/message_returns/task_message_return_validation.cpp
    src/resident/domains/tasks/message_returns/task_message_return_view.cpp
    src/resident/domains/tasks/message_returns/task_message_return_world.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_boundary.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_catalog.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_codec.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_control.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_finish.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_preview.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_profile.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_progress.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_receipts.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_services.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_validation.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_view.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_world.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_boundary.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_catalog.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_control.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_codec.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_finish.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_preview.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_profile.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_progress.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_receipts.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_services.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_validation.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_view.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_world.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_catalog.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_codec.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_control.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_finish.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_preview.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_profile.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_progress.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_receipts.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_services.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_validation.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_view.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_catalog.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_codec.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_control.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_preview.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_profile.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_receipts.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_validation.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_view.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_progress.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_settlement.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_sources.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_validation.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_view.cpp
    src/resident/domains/tasks/task_fulfilled_clients.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_employment_view.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_employment_view_cpp
    src/resident/domains/tasks/task_employment_view.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_completion_evidence.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_completion_evidence_cpp
    src/resident/domains/tasks/task_completion_evidence.cpp
    src/resident/domains/tasks/task_completion_evidence.hpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_checkpoint_cpp
    src/resident/domains/tasks/task_checkpoint.cpp
    src/resident/domains/tasks/task_codec.cpp
    src/resident/domains/tasks/task_journal.cpp
    src/resident/domains/tasks/envoy_task_admission.cpp
    src/resident/domains/tasks/envoy_task_rules.cpp
    src/resident/domains/tasks/envoy_task_codec.cpp
    src/resident/domains/tasks/envoy_task_contact.cpp
    src/resident/domains/tasks/envoy_task_actions.cpp
    src/resident/domains/tasks/envoy_task_completion.cpp
    src/resident/domains/tasks/envoy_task_cancellation.cpp
    src/resident/domains/tasks/envoy_task_step.cpp
    src/resident/domains/tasks/envoy_task_boundary.cpp
    src/resident/domains/tasks/envoy_task_validation.cpp
    src/resident/domains/tasks/envoy_task_view.cpp
    src/resident/domains/tasks/envoy_task_payloads.cpp
    src/resident/domains/tasks/envoy_task_report_contact.cpp
    src/resident/domains/tasks/envoy_task_report_reference.cpp
    src/resident/domains/tasks/envoy_task_report_rules.cpp
    src/resident/domains/tasks/envoy_task_report_step.cpp
    src/resident/domains/tasks/envoy_task_version.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/governance_task_rules.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_governance_task_rules_cpp
    src/resident/domains/tasks/governance_task_rules.cpp
    src/resident/domains/tasks/governance_task_codec.cpp
    src/resident/domains/tasks/governance_task_admission.cpp
    src/resident/domains/tasks/governance_task_step.cpp
    src/resident/domains/tasks/governance_task_view.cpp
    src/resident/domains/tasks/governance_task_payloads.cpp
    src/resident/domains/tasks/task_lifecycle_view.cpp
    src/resident/domains/tasks/task_execution_gate.cpp
    src/resident/domains/tasks/task_offer_response.cpp
    src/resident/domains/tasks/task_personal_advance.cpp
    src/resident/domains/tasks/task_personal_advance_admission.cpp
    src/resident/domains/tasks/task_personal_advance_catalog.cpp
    src/resident/domains/tasks/task_personal_advance_codec.cpp
    src/resident/domains/tasks/task_personal_advance_command.cpp
    src/resident/domains/tasks/task_personal_advance_profile.cpp
    src/resident/domains/tasks/task_personal_advance_view.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_runtime_offer_response.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_runtime_offer_response_cpp
    src/resident/domains/tasks/task_runtime_offer_response.cpp
    src/resident/domains/tasks/task_runtime_offer_response_codec.cpp
    src/resident/domains/tasks/task_runtime_offer_view.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_offer_response_codec.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_offer_response_codec_cpp
    src/resident/domains/tasks/task_offer_response_codec.cpp
    src/resident/domains/tasks/task_offer_response_profile.cpp
    src/resident/domains/tasks/task_offer_revisit.cpp
    src/resident/domains/tasks/task_travel_manifest.cpp
    src/resident/domains/tasks/task_route_obligations.cpp
    src/resident/domains/tasks/task_travel_manifest_codec.cpp
    src/resident/domains/tasks/task_rules.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/message_notices/task_message_notice_world.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_message_notice_world_cpp
    src/resident/domains/tasks/message_notices/task_message_notice_world.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_view.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_validation.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_rules.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_codec.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_catalog.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_codec.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_profile.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_rules.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_validation.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_view.cpp
    src/resident/domains/tasks/message_handoffs/task_message_report_validation.cpp
    src/resident/domains/tasks/message_handoffs/task_message_report_rules.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_static_public_payment.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_static_public_payment_cpp
    src/resident/domains/tasks/task_static_public_payment.cpp
    src/resident/domains/tasks/task_static_public_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/objectives/task_objective_evidence.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_objective_evidence_cpp
    src/resident/domains/tasks/objectives/task_objective_evidence.cpp
    src/resident/domains/tasks/objectives/task_objective_evidence_codec.cpp
    src/resident/domains/tasks/state_need_task.cpp
    src/resident/domains/tasks/state_need_task_codec.cpp
    src/resident/domains/tasks/task_claim_establishment.cpp
    src/resident/domains/tasks/task_claim_establishment_codec.cpp
    src/resident/domains/tasks/task_quantity_time2.cpp
    src/resident/domains/tasks/task_quantity_time2_codec.cpp
    src/resident/domains/tasks/task_complete_action_time.cpp
    src/resident/domains/tasks/task_exact_execution.cpp
    src/resident/domains/tasks/task_exact_memory.cpp
    src/resident/domains/tasks/task_exact_life.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_exact_execution_codec.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_exact_execution_codec_cpp
    src/resident/domains/tasks/task_exact_execution_codec.cpp
    src/resident/domains/tasks/task_exact_phase.cpp
    src/resident/domains/tasks/task_exact_read_clock.cpp
    src/resident/domains/tasks/task_original_sources.cpp
    src/resident/domains/tasks/task_result_time2.cpp
    src/resident/domains/tasks/task_transport_agreement.cpp
    src/resident/domains/tasks/task_transport_agreement_codec.cpp
    src/resident/domains/tasks/work_execution/work_commission_terminal.cpp
    src/resident/domains/tasks/work_execution/work_commission_terminal_codec.cpp
    src/resident/domains/tasks/work_execution/work_exact_runtime.cpp
    src/resident/domains/tasks/work_execution/work_execution.cpp
    src/resident/domains/tasks/work_execution/work_execution_codec.cpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms_codec.cpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms.cpp
    src/resident/domains/tasks/work_execution/work_execution_read.cpp
    src/resident/domains/tasks/work_execution/work_expense_acceptance.cpp
    src/resident/domains/tasks/work_execution/work_follow_through.cpp
    src/resident/domains/tasks/work_execution/work_follow_through_codec.cpp
    src/resident/domains/tasks/work_execution/work_military_sources.cpp
    src/resident/domains/tasks/work_execution/work_result_projection.cpp
    src/resident/domains/tasks/work_execution/work_supervision_original.cpp
    src/resident/domains/tasks/work_execution/work_supervision_sources.cpp
    src/resident/domains/tasks/work_execution/work_reassignment_sources.cpp
    src/resident/domains/tasks/work_execution/work_lifecycle_original.cpp
    src/resident/domains/tasks/work_execution/work_task_sources.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/work_execution/work_patrol_sources.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_work_patrol_sources_cpp
    src/resident/domains/tasks/work_execution/work_patrol_sources.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/work_execution/work_original_read.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_work_original_read_cpp
    src/resident/domains/tasks/work_execution/work_original_read.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/work_execution/work_skill_inputs.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_work_skill_inputs_cpp
    src/resident/domains/tasks/work_execution/work_skill_inputs.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/task_report_carriage.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_task_report_carriage_cpp
    src/resident/domains/tasks/task_report_carriage.cpp
    src/resident/domains/tasks/task_report_carriage_command.cpp
    src/resident/domains/tasks/task_report_carrier_acceptance.cpp
    src/resident/domains/tasks/task_report_carrier_catalog.cpp
    src/resident/domains/tasks/task_report_carrier_scope.cpp
    src/resident/domains/tasks/task_report_carrier_terms.cpp
    src/resident/domains/tasks/task_report_propagation.cpp
    src/resident/domains/tasks/work_execution/work_economic_window_source.cpp
    src/resident/domains/tasks/work_execution/work_growth_contribution.cpp
)

# 生产输入连续登记段：src/resident/domains/tasks/shop_orders/professional_order_codec.cpp。
set(HANMO_RESIDENT_SOURCES_tasks_professional_order_codec_cpp
    src/resident/domains/tasks/shop_orders/professional_order_codec.cpp
    src/resident/domains/tasks/shop_orders/professional_order_contract.cpp
    src/resident/domains/tasks/task_quantity_deadline.cpp
    src/resident/domains/tasks/task_quantity_deadline_codec.cpp
    src/resident/domains/tasks/work_execution/work_native_lease.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/message_notices/task_message_notice_history.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_message_notice_history_hpp
    src/resident/domains/tasks/message_notices/task_message_notice_history.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_actor_pending_responsibilities.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_actor_pending_responsibilities_cpp
    src/resident/domains/tasks/task_actor_pending_responsibilities.cpp
    src/resident/domains/tasks/task_inheritance_history.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_rules.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_codec.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_validation.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_cash_sources.cpp
    src/resident/domains/tasks/static_assets/task_static_asset_view.cpp
    src/resident/domains/tasks/extensions/task_extension_catalog.cpp
    src/resident/domains/tasks/extensions/task_extension_codec.cpp
    src/resident/domains/tasks/extensions/task_extension_rules.cpp
    src/resident/domains/tasks/extensions/task_extension_validation.cpp
    src/resident/domains/tasks/extensions/task_extension_boundary.cpp
    src/resident/domains/tasks/extensions/task_extension_view.cpp
    src/resident/domains/tasks/extensions/task_extension_world.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_authored_continuation.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_authored_continuation_cpp
    src/resident/domains/tasks/task_authored_continuation.cpp
    src/resident/domains/tasks/task_responsibility_continuation.cpp
    src/resident/domains/tasks/task_responsibility_director.cpp
    src/resident/domains/tasks/task_responsibility_director_world.cpp
    src/resident/domains/tasks/task_authored_continuation_catalog.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_actor_pending_responsibilities.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_actor_pending_responsibilities_hpp
    src/resident/domains/tasks/task_actor_pending_responsibilities.hpp
    src/resident/domains/tasks/task_inheritance_history.hpp
    src/resident/domains/tasks/static_assets/task_static_asset.hpp
    src/resident/domains/tasks/static_assets/task_static_asset_state.hpp
    src/resident/domains/tasks/static_assets/task_static_asset_view.hpp
    src/resident/domains/tasks/extensions/task_extension.hpp
    src/resident/domains/tasks/extensions/task_extension_state.hpp
    src/resident/domains/tasks/extensions/task_extension_boundary.hpp
    src/resident/domains/tasks/extensions/task_extension_view.hpp
    src/resident/domains/tasks/extensions/task_extension_world.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_authored_continuation.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_authored_continuation_hpp
    src/resident/domains/tasks/task_authored_continuation.hpp
    src/resident/domains/tasks/task_responsibility_continuation.hpp
    src/resident/domains/tasks/task_responsibility_director.hpp
    src/resident/domains/tasks/task_responsibility_director_world.hpp
    src/resident/domains/tasks/task_authored_continuation_catalog.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/message_notices/task_message_notice_world.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_message_notice_world_hpp
    src/resident/domains/tasks/message_notices/task_message_notice_world.hpp
    src/resident/domains/tasks/message_notices/task_message_notice_state.hpp
    src/resident/domains/tasks/message_notices/task_message_notice_catalog.hpp
    src/resident/domains/tasks/message_notices/task_message_notice.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/message_handoffs/task_message_handoff.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_message_handoff_hpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff.hpp
    src/resident/domains/tasks/message_handoffs/task_message_report.hpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_profile.hpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_state.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/governance_task_rules.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_governance_task_rules_hpp
    src/resident/domains/tasks/governance_task_rules.hpp
    src/resident/domains/tasks/governance_task_step.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_public_account_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_public_account_codec_cpp
    src/resident/domains/tasks/task_public_account_codec.cpp
    src/resident/domains/tasks/task_public_account_context.cpp
    src/resident/domains/tasks/task_public_account_context.hpp
    src/resident/domains/tasks/task_public_account_dependencies.cpp
    src/resident/domains/tasks/task_public_account_dependencies.hpp
    src/resident/domains/tasks/task_public_account_execution.cpp
    src/resident/domains/tasks/task_public_account_execution.hpp
    src/resident/domains/tasks/task_public_account_payment.cpp
    src/resident/domains/tasks/task_public_account_payment.hpp
    src/resident/domains/tasks/task_public_account_phase.hpp
    src/resident/domains/tasks/task_public_account_reservations.cpp
    src/resident/domains/tasks/task_public_account_reservations.hpp
    src/resident/domains/tasks/task_public_account_state.hpp
    src/resident/domains/tasks/task_public_account_view.cpp
    src/resident/domains/tasks/task_public_account_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_dynamic_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_dynamic_codec_cpp
    src/resident/domains/tasks/task_dynamic_codec.cpp
    src/resident/domains/tasks/task_dynamic_codec.hpp
    src/resident/domains/tasks/task_dynamic_rules.cpp
    src/resident/domains/tasks/objectives/objective_result_committed.cpp
    src/resident/domains/tasks/objectives/objective_result_committed.hpp
    src/resident/domains/tasks/task_dynamic_cargo.cpp
    src/resident/domains/tasks/task_dynamic_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_order_estate.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_shop_order_estate_hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_boundary.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_state.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_order_estate.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_shop_order_estate_cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_boundary.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_codec.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_estate_receipts.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_shop_asset_responsibilities_hpp
    src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_boundary.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_business.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_receipt.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_source.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_contract.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_execution.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_progress.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_sources.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_state.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_state.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_contract.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contacts.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_detail.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_receipts.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_profile.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_control.hpp
    src/resident/domains/tasks/message_returns/task_message_return.hpp
    src/resident/domains/tasks/message_returns/task_message_return_boundary.hpp
    src/resident/domains/tasks/message_returns/task_message_return_catalog.hpp
    src/resident/domains/tasks/message_returns/task_message_return_contract.hpp
    src/resident/domains/tasks/message_returns/task_message_return_control.hpp
    src/resident/domains/tasks/message_returns/task_message_return_detail.hpp
    src/resident/domains/tasks/message_returns/task_message_return_profile.hpp
    src/resident/domains/tasks/message_returns/task_message_return_progress.hpp
    src/resident/domains/tasks/message_returns/task_message_return_receipts.hpp
    src/resident/domains/tasks/message_returns/task_message_return_report.hpp
    src/resident/domains/tasks/message_returns/task_message_return_services.hpp
    src/resident/domains/tasks/message_returns/task_message_return_state.hpp
    src/resident/domains/tasks/message_returns/task_message_return_view.hpp
    src/resident/domains/tasks/message_returns/task_message_return_world.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_contract.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_control.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_detail.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_profile.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_progress.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_receipts.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_services.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_state.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_view.hpp
    src/resident/domains/tasks/personal_messages/task_personal_message_world.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_catalog.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_control.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_contract.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_detail.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_profile.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_progress.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_receipts.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_services.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_state.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_view.hpp
    src/resident/domains/tasks/personal_reports/task_personal_report_world.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_catalog.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_contract.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_control.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_detail.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_profile.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_progress.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_receipts.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_services.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_state.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_view.hpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_catalog.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_contract.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_control.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_detail.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_profile.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_receipts.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_state.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_view.hpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp
    src/resident/domains/tasks/shop_orders/task_shop_order_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_shop_asset_responsibilities_cpp
    src/resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_acceptance.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_actions.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_boundary.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_business.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_receipt.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_cached_source.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_codec.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_codec.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_rules.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_validation.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_receipts.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_access.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_profile.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.cpp
    src/resident/domains/tasks/shop_order_contacts/task_shop_order_contact_control.cpp
    src/resident/domains/tasks/message_returns/task_message_return_boundary.cpp
    src/resident/domains/tasks/message_returns/task_message_return_catalog.cpp
    src/resident/domains/tasks/message_returns/task_message_return_codec.cpp
    src/resident/domains/tasks/message_returns/task_message_return_collection.cpp
    src/resident/domains/tasks/message_returns/task_message_return_control.cpp
    src/resident/domains/tasks/message_returns/task_message_return_delivery.cpp
    src/resident/domains/tasks/message_returns/task_message_return_finish.cpp
    src/resident/domains/tasks/message_returns/task_message_return_profile.cpp
    src/resident/domains/tasks/message_returns/task_message_return_progress.cpp
    src/resident/domains/tasks/message_returns/task_message_return_receipts.cpp
    src/resident/domains/tasks/message_returns/task_message_return_report_start.cpp
    src/resident/domains/tasks/message_returns/task_message_return_report_validation.cpp
    src/resident/domains/tasks/message_returns/task_message_return_services.cpp
    src/resident/domains/tasks/message_returns/task_message_return_validation.cpp
    src/resident/domains/tasks/message_returns/task_message_return_view.cpp
    src/resident/domains/tasks/message_returns/task_message_return_world.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_boundary.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_catalog.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_codec.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_control.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_finish.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_preview.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_profile.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_progress.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_receipts.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_services.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_validation.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_view.cpp
    src/resident/domains/tasks/personal_messages/task_personal_message_world.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_boundary.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_catalog.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_control.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_codec.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_finish.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_preview.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_profile.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_progress.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_receipts.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_services.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_validation.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_view.cpp
    src/resident/domains/tasks/personal_reports/task_personal_report_world.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_catalog.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_codec.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_control.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_finish.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_preview.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_profile.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_progress.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_receipts.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_services.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_validation.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_view.cpp
    src/resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_catalog.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_codec.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_control.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_preview.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_profile.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_receipts.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_validation.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_view.cpp
    src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_progress.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_settlement.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_sources.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_validation.cpp
    src/resident/domains/tasks/shop_orders/task_shop_order_view.cpp
    src/resident/domains/tasks/task_fulfilled_clients.cpp
    src/resident/domains/tasks/task_fulfilled_clients.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_employment_view.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_employment_view_hpp
    src/resident/domains/tasks/task_employment_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_employment_view.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_employment_view_cpp
    src/resident/domains/tasks/task_employment_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_completion_evidence.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_completion_evidence_cpp
    src/resident/domains/tasks/task_completion_evidence.cpp
    src/resident/domains/tasks/task_completion_evidence.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_checkpoint_cpp
    src/resident/domains/tasks/task_checkpoint.cpp
    src/resident/domains/tasks/task_codec.cpp
    src/resident/domains/tasks/task_journal.cpp
    src/resident/domains/tasks/envoy_task_admission.cpp
    src/resident/domains/tasks/envoy_task_rules.cpp
    src/resident/domains/tasks/envoy_task_codec.cpp
    src/resident/domains/tasks/envoy_task_contact.cpp
    src/resident/domains/tasks/envoy_task_actions.cpp
    src/resident/domains/tasks/envoy_task_completion.cpp
    src/resident/domains/tasks/envoy_task_cancellation.cpp
    src/resident/domains/tasks/envoy_task_step.cpp
    src/resident/domains/tasks/envoy_task_boundary.cpp
    src/resident/domains/tasks/envoy_task_validation.cpp
    src/resident/domains/tasks/envoy_task_view.cpp
    src/resident/domains/tasks/envoy_task_payloads.cpp
    src/resident/domains/tasks/envoy_task_report_contact.cpp
    src/resident/domains/tasks/envoy_task_report_reference.cpp
    src/resident/domains/tasks/envoy_task_report_rules.cpp
    src/resident/domains/tasks/envoy_task_report_step.cpp
    src/resident/domains/tasks/envoy_task_version.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/governance_task_rules.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_governance_task_rules_cpp
    src/resident/domains/tasks/governance_task_rules.cpp
    src/resident/domains/tasks/governance_task_codec.cpp
    src/resident/domains/tasks/governance_task_admission.cpp
    src/resident/domains/tasks/governance_task_step.cpp
    src/resident/domains/tasks/governance_task_view.cpp
    src/resident/domains/tasks/governance_task_payloads.cpp
    src/resident/domains/tasks/task_lifecycle_view.cpp
    src/resident/domains/tasks/task_execution_gate.cpp
    src/resident/domains/tasks/task_offer_response.cpp
    src/resident/domains/tasks/task_personal_advance.cpp
    src/resident/domains/tasks/task_personal_advance_admission.cpp
    src/resident/domains/tasks/task_personal_advance_catalog.cpp
    src/resident/domains/tasks/task_personal_advance_codec.cpp
    src/resident/domains/tasks/task_personal_advance_command.cpp
    src/resident/domains/tasks/task_personal_advance_profile.cpp
    src/resident/domains/tasks/task_personal_advance_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_runtime_offer_response.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_runtime_offer_response_cpp
    src/resident/domains/tasks/task_runtime_offer_response.cpp
    src/resident/domains/tasks/task_runtime_offer_response_codec.cpp
    src/resident/domains/tasks/task_runtime_offer_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_offer_response_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_offer_response_codec_cpp
    src/resident/domains/tasks/task_offer_response_codec.cpp
    src/resident/domains/tasks/task_offer_response_profile.cpp
    src/resident/domains/tasks/task_offer_revisit.cpp
    src/resident/domains/tasks/task_travel_manifest.cpp
    src/resident/domains/tasks/task_route_obligations.cpp
    src/resident/domains/tasks/task_travel_manifest_codec.cpp
    src/resident/domains/tasks/task_lifecycle_view.hpp
    src/resident/domains/tasks/envoy_task_contract.hpp
    src/resident/domains/tasks/envoy_task_state.hpp
    src/resident/domains/tasks/envoy_task_admission.hpp
    src/resident/domains/tasks/envoy_task_rules.hpp
    src/resident/domains/tasks/envoy_task_contact.hpp
    src/resident/domains/tasks/envoy_task_actions.hpp
    src/resident/domains/tasks/envoy_task_completion.hpp
    src/resident/domains/tasks/envoy_task_cancellation.hpp
    src/resident/domains/tasks/envoy_task_step.hpp
    src/resident/domains/tasks/envoy_task_boundary.hpp
    src/resident/domains/tasks/envoy_task_validation.hpp
    src/resident/domains/tasks/envoy_task_view.hpp
    src/resident/domains/tasks/envoy_task_payloads.hpp
    src/resident/domains/tasks/envoy_task_report_contact.hpp
    src/resident/domains/tasks/envoy_task_report_reference.hpp
    src/resident/domains/tasks/envoy_task_report_rules.hpp
    src/resident/domains/tasks/envoy_task_report_step.hpp
    src/resident/domains/tasks/envoy_task_version.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_execution_gate.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_execution_gate_hpp
    src/resident/domains/tasks/task_execution_gate.hpp
    src/resident/domains/tasks/task_offer_response.hpp
    src/resident/domains/tasks/task_personal_advance.hpp
    src/resident/domains/tasks/task_personal_advance_admission.hpp
    src/resident/domains/tasks/task_personal_advance_catalog.hpp
    src/resident/domains/tasks/task_personal_advance_command.hpp
    src/resident/domains/tasks/task_personal_advance_profile.hpp
    src/resident/domains/tasks/task_personal_advance_rules.hpp
    src/resident/domains/tasks/task_personal_advance_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_runtime_offer_response.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_runtime_offer_response_hpp
    src/resident/domains/tasks/task_runtime_offer_response.hpp
    src/resident/domains/tasks/task_runtime_offer_view.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_offer_response_profile.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_offer_response_profile_hpp
    src/resident/domains/tasks/task_offer_response_profile.hpp
    src/resident/domains/tasks/task_offer_revisit.hpp
    src/resident/domains/tasks/task_route_obligations.hpp
    src/resident/domains/tasks/task_travel_manifest.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_model.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_model_hpp
    src/resident/domains/tasks/task_model.hpp
    src/resident/domains/tasks/task_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/message_notices/task_message_notice_world.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_message_notice_world_cpp
    src/resident/domains/tasks/message_notices/task_message_notice_world.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_view.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_validation.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_rules.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_codec.cpp
    src/resident/domains/tasks/message_notices/task_message_notice_catalog.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_codec.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_profile.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_rules.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_validation.cpp
    src/resident/domains/tasks/message_handoffs/task_message_handoff_view.cpp
    src/resident/domains/tasks/message_handoffs/task_message_report_validation.cpp
    src/resident/domains/tasks/message_handoffs/task_message_report_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_static_public_payment.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_static_public_payment_cpp
    src/resident/domains/tasks/task_static_public_payment.cpp
    src/resident/domains/tasks/task_static_public_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_static_public_payment.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_static_public_payment_hpp
    src/resident/domains/tasks/task_static_public_payment.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/objectives/task_objective_evidence.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_objective_evidence_cpp
    src/resident/domains/tasks/objectives/task_objective_evidence.cpp
    src/resident/domains/tasks/objectives/task_objective_evidence_codec.cpp
    src/resident/domains/tasks/state_need_task.cpp
    src/resident/domains/tasks/state_need_task_codec.cpp
    src/resident/domains/tasks/task_claim_establishment.cpp
    src/resident/domains/tasks/task_claim_establishment_codec.cpp
    src/resident/domains/tasks/task_quantity_time2.cpp
    src/resident/domains/tasks/task_quantity_time2_codec.cpp
    src/resident/domains/tasks/task_complete_action_time.cpp
    src/resident/domains/tasks/task_exact_execution.cpp
    src/resident/domains/tasks/task_exact_memory.cpp
    src/resident/domains/tasks/task_exact_life.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/task_exact_execution_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_task_exact_execution_codec_cpp
    src/resident/domains/tasks/task_exact_execution_codec.cpp
    src/resident/domains/tasks/task_exact_phase.cpp
    src/resident/domains/tasks/task_exact_read_clock.cpp
    src/resident/domains/tasks/task_original_sources.cpp
    src/resident/domains/tasks/task_report_carriage.cpp
    src/resident/domains/tasks/task_report_carriage_command.cpp
    src/resident/domains/tasks/task_report_carrier_acceptance.cpp
    src/resident/domains/tasks/task_report_carrier_catalog.cpp
    src/resident/domains/tasks/task_report_carrier_scope.cpp
    src/resident/domains/tasks/task_report_carrier_terms.cpp
    src/resident/domains/tasks/task_report_propagation.cpp
    src/resident/domains/tasks/task_result_time2.cpp
    src/resident/domains/tasks/task_transport_agreement.cpp
    src/resident/domains/tasks/task_transport_agreement_codec.cpp
    src/resident/domains/tasks/work_execution/work_commission_terminal.cpp
    src/resident/domains/tasks/work_execution/work_commission_terminal_codec.cpp
    src/resident/domains/tasks/work_execution/work_economic_window_source.cpp
    src/resident/domains/tasks/work_execution/work_exact_runtime.cpp
    src/resident/domains/tasks/work_execution/work_execution.cpp
    src/resident/domains/tasks/work_execution/work_execution_codec.cpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms_codec.cpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms.cpp
    src/resident/domains/tasks/work_execution/work_execution_read.cpp
    src/resident/domains/tasks/work_execution/work_expense_acceptance.cpp
    src/resident/domains/tasks/work_execution/work_follow_through.cpp
    src/resident/domains/tasks/work_execution/work_follow_through_codec.cpp
    src/resident/domains/tasks/work_execution/work_growth_contribution.cpp
    src/resident/domains/tasks/work_execution/work_military_sources.cpp
    src/resident/domains/tasks/work_execution/work_original_read.cpp
    src/resident/domains/tasks/work_execution/work_patrol_sources.cpp
    src/resident/domains/tasks/work_execution/work_result_projection.cpp
    src/resident/domains/tasks/work_execution/work_skill_inputs.cpp
    src/resident/domains/tasks/work_execution/work_supervision_original.cpp
    src/resident/domains/tasks/work_execution/work_supervision_sources.cpp
    src/resident/domains/tasks/work_execution/work_reassignment_sources.cpp
    src/resident/domains/tasks/work_execution/work_lifecycle_original.cpp
    src/resident/domains/tasks/work_execution/work_task_sources.cpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/state_need_task.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_state_need_task_hpp
    src/resident/domains/tasks/state_need_task.hpp
    src/resident/domains/tasks/task_complete_action_time.hpp
    src/resident/domains/tasks/task_exact_execution.hpp
    src/resident/domains/tasks/task_projection.hpp
    src/resident/domains/tasks/task_report_carriage.hpp
    src/resident/domains/tasks/task_report_carriage_codec_fields.hpp
    src/resident/domains/tasks/task_report_carriage_command.hpp
    src/resident/domains/tasks/task_report_carriage_types.hpp
    src/resident/domains/tasks/task_report_carrier_acceptance.hpp
    src/resident/domains/tasks/task_report_carrier_catalog.hpp
    src/resident/domains/tasks/task_report_carrier_scope.hpp
    src/resident/domains/tasks/task_report_carrier_terms.hpp
    src/resident/domains/tasks/task_report_propagation.hpp
    src/resident/domains/tasks/task_source_projection.hpp
    src/resident/domains/tasks/task_transport_agreement.hpp
    src/resident/domains/tasks/work_execution/work_economic_window_source.hpp
    src/resident/domains/tasks/work_execution/work_exact_runtime.hpp
    src/resident/domains/tasks/work_execution/work_execution_contract.hpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms_fields.hpp
    src/resident/domains/tasks/work_execution/work_supervision_execution_terms.hpp
    src/resident/domains/tasks/work_execution/work_execution_detail.hpp
    src/resident/domains/tasks/work_execution/work_growth_contribution.hpp
    src/resident/domains/tasks/work_execution/work_original_read.hpp
    src/resident/domains/tasks/work_execution/work_patrol_sources.hpp
    src/resident/domains/tasks/work_execution/work_skill_inputs.hpp
    src/resident/domains/tasks/work_execution/work_supervision_original.hpp
    src/resident/domains/tasks/work_execution/work_supervision_sources.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/work_execution/work_reassignment_sources.hpp。
set(HANMO_RESIDENT_IDENTITY_tasks_work_reassignment_sources_hpp
    src/resident/domains/tasks/work_execution/work_reassignment_sources.hpp
    src/resident/domains/tasks/work_execution/work_lifecycle_original.hpp
)

# 生产身份输入连续登记段：src/resident/domains/tasks/shop_orders/professional_order_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_tasks_professional_order_codec_cpp
    src/resident/domains/tasks/shop_orders/professional_order_codec.cpp
    src/resident/domains/tasks/shop_orders/professional_order_contract.cpp
    src/resident/domains/tasks/task_quantity_deadline.cpp
    src/resident/domains/tasks/task_quantity_deadline_codec.cpp
    src/resident/domains/tasks/work_execution/work_native_lease.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/tasks/source_owned_fields/contracts_messages.inc
    src/resident/domains/tasks/source_owned_fields/execution_results.inc
    src/resident/domains/tasks/world_codec/catalog_governance.inc
    src/resident/domains/tasks/world_codec/catalog_offer_profiles.inc
    src/resident/domains/tasks/world_codec/catalog_original_extensions.inc
    src/resident/domains/tasks/world_codec/catalog_personal_advance.inc
    src/resident/domains/tasks/world_codec/catalog_personal_contracts.inc
    src/resident/domains/tasks/world_codec/catalog_regional_sources.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_governance_tasks.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_task_message_return_module.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_task_shop_order_contact_module.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_task_shop_order_estate_cancellations.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_task_shop_orders.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_task_takeover_execution.inc
    src/resident/domains/tasks/world_codec/checkpoint_read_tasks.inc
    src/resident/domains/tasks/world_codec/checkpoint_write_dynamic.inc
    src/resident/domains/tasks/world_codec/checkpoint_write_exact_execution.inc
    src/resident/domains/tasks/world_codec/checkpoint_write_work.inc
    src/resident/domains/tasks/world_codec/export_exact_execution.inc
    src/resident/domains/tasks/world_codec/export_work.inc
    src/resident/domains/tasks/world_codec/export_work_action_payloads.inc
    src/resident/domains/tasks/world_codec/seed_dynamic.inc
    src/resident/domains/tasks/world_codec/seed_envoy_governance.inc
    src/resident/domains/tasks/world_codec/seed_original_execution.inc
    src/resident/domains/tasks/world_codec/seed_personal_contracts.inc
    src/resident/domains/tasks/world_codec/seed_shop_contacts.inc
    src/resident/domains/tasks/world_codec/seed_validate_personal_payloads.inc
    src/resident/domains/tasks/world_codec/seed_validate_shop_contacts.inc
    src/resident/domains/tasks/world_codec/seed_work_contracts.inc
)
