// Original JSON and bounded projection share these exact borrowed field writers.
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
class CommonStore;struct ActionCoreView;struct FactRecord;
struct TaskMessageReturnState;
struct TaskPersonalAcceptanceState;
struct TaskPersonalCancellationState;
struct TaskPersonalMessageState;
struct TaskPersonalReportState;
struct TaskShopOrderAction;
struct TaskShopOrderBinding;
struct TaskShopOrderContactState;
struct TaskShopOrderEstateCancellationState;
struct TaskShopOrderInterruption;
struct TaskShopOrderSettlement;
struct TaskShopOrderState;

Outcome<bool> emit_task_message_return_action_payload_fields(ProjectionWriter&,const TaskMessageReturnState*,const ActionCoreView&);
// Per-Common-row fields. Successful false leaves the sink untouched.
Outcome<bool> emit_task_shop_order_contact_fact_payload_fields(ProjectionWriter&,const TaskShopOrderContactState*,const FactRecord&);
Outcome<bool> emit_task_personal_cancellation_fact_payload_fields(ProjectionWriter&,const TaskPersonalCancellationState*,const FactRecord&);
Outcome<bool> emit_task_personal_report_fact_payload_fields(ProjectionWriter&,const TaskPersonalReportState*,const FactRecord&);
Outcome<bool> emit_task_message_return_fact_payload_fields(ProjectionWriter&,const TaskMessageReturnState*,const FactRecord&);
Outcome<bool> emit_task_personal_message_fact_payload_fields(ProjectionWriter&,const TaskPersonalMessageState*,const FactRecord&);
Outcome<bool> emit_task_personal_acceptance_fact_payload_fields(ProjectionWriter&,const TaskPersonalAcceptanceState*,const FactRecord&);

Status emit_task_shop_order_contacts_fields(ProjectionWriter&,const TaskShopOrderContactState&);
Status emit_task_shop_order_contact_fact_fields(ProjectionWriter&,const TaskShopOrderContactState&,const StableId&);
Status emit_task_personal_messages_fields(ProjectionWriter&,const TaskPersonalMessageState&);
Status emit_task_personal_message_fact_fields(ProjectionWriter&,const TaskPersonalMessageState&,const StableId&);
Status emit_task_personal_reports_fields(ProjectionWriter&,const TaskPersonalReportState&);
Status emit_task_personal_report_fact_fields(ProjectionWriter&,const TaskPersonalReportState&,const StableId&);
Status emit_task_personal_acceptances_fields(ProjectionWriter&,const TaskPersonalAcceptanceState&);
Status emit_task_personal_acceptance_fact_fields(ProjectionWriter&,const TaskPersonalAcceptanceState&,const StableId&);
Status emit_task_personal_cancellations_fields(ProjectionWriter&,const TaskPersonalCancellationState&);
Status emit_task_personal_cancellation_fact_fields(ProjectionWriter&,const TaskPersonalCancellationState&,const StableId&);
Status emit_task_message_returns_fields(ProjectionWriter&,const TaskMessageReturnState&);
Status emit_task_message_return_fact_fields(ProjectionWriter&,const TaskMessageReturnState&,const StableId&);
Status emit_task_shop_order_estate_cancellations_fields(ProjectionWriter&,const TaskShopOrderEstateCancellationState&);
Status emit_task_shop_order_estate_fact_fields(ProjectionWriter&,const TaskShopOrderEstateCancellationState&,const StableId&);
Status emit_task_shop_order_binding_fields(ProjectionWriter&,const TaskShopOrderBinding&);
Status emit_task_shop_order_settlement_fields(ProjectionWriter&,const TaskShopOrderSettlement&);
Status emit_task_shop_order_interruption_fields(ProjectionWriter&,const TaskShopOrderInterruption&);
Status emit_task_shop_order_action_fields(ProjectionWriter&,const TaskShopOrderAction&);
Status emit_task_shop_orders_fields(ProjectionWriter&,const TaskShopOrderState&);
Status emit_task_message_return_action_fields(ProjectionWriter&,const TaskMessageReturnState&,const StableId&);
Status emit_task_personal_acceptance_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskPersonalAcceptanceState* state);

Status emit_task_personal_message_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskPersonalMessageState* state);

Status emit_task_personal_report_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskPersonalReportState* state);

Status emit_task_personal_cancellation_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskPersonalCancellationState* state);

Status emit_task_message_return_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskMessageReturnState* state);

Status emit_task_message_return_action_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskMessageReturnState* state);

Status emit_task_shop_order_contact_fact_payloads_fields(ProjectionWriter& w,const CommonStore& c,const TaskShopOrderContactState* state);

}







