// History2唯一声明字段序；JSON的FieldOrder仍是原对象顺序，binary保原显式尾。
#pragma once
#include "growth_market_history.hpp"
namespace hanmo::resident::growth_market_v2_fields {
// 枚举只承载已登记数字，不接受自由字符串或越界tag。
template <class A, class T>
void tag(A &a, const char *name, T &value, Integer first, Integer last) {
  Integer encoded = static_cast<Integer>(value);
  a(name, encoded);
  if (!a.status().ok)
    return;
  if (encoded < first || encoded > last) {
    a.reject("InvalidMarketHistory2Enum", name);
    return;
  }
  if (a.reading())
    value = static_cast<T>(encoded);
}
template <class A> void fields(A &, MarketLegacyOriginalStep &) {}
template <class A> void fields(A &a, MarketSelectedTradeRows &v) {
  a("life_activity_id", v.life_activity_id);
  a("source_schema", v.source_schema);
  a("row_ordinals", v.row_ordinals);
}
template <class A> void fields(A &a, MarketOriginalBusinessFifo &v) {
  a("business_fact_id", v.business_fact_id);
  a("execution_stage_ordinal", v.execution_stage_ordinal);
}
template <class A> void fields(A &, MarketOriginalTick &) {}
template <class A> void fields(A &a, MarketScalarActualStart &v) {
  a("started_at", v.started_at);
  a("action_ref", v.action_ref);
}
template <class A> void fields(A &, MarketProtectedHistoryBefore &) {}
template <class A> void fields(A &a, MarketFullCapturedBefore &v) {
  a("inventory", v.inventory);
}
template <class A> void fields(A &a, MarketPriorPinnedStageAfter &v) {
  a("movement_id", v.movement_id);
  a("execution_stage_ordinal", v.execution_stage_ordinal);
}
template <class A> void fields(A &a, OriginalBusinessInventoryFrame &v) {
  a("inventory_id", v.inventory_id);
  a("before_basis", v.before_basis);
  a("after_delta", v.after_delta);
  a.order(v.field_order);
}
template <class A> void fields(A &, MarketScalarTradeSlot &) {}
template <class A> void fields(A &a, MarketProductionInputSlot &v) {
  a("receipt_input_ordinal", v.receipt_input_ordinal);
}
template <class A> void fields(A &a, MarketProductionOutputSlot &v) {
  a("receipt_output_ordinal", v.receipt_output_ordinal);
}
template <class A> void fields(A &, MarketRationConsumptionSlot &) {}
template <class A> void fields(A &, MarketNotConsumption &) {}
template <class A> void fields(A &a, MarketOriginalMaterialNeed &v) {
  a("rule_ref", v.rule_ref);
  a("need_slot_ordinal", v.need_slot_ordinal);
}
template <class A> void fields(A &, MarketNotOutput &) {}
template <class A> void fields(A &, MarketOriginalPlainAdd &) {}
template <class A> void fields(A &a, MarketRegisteredProductionOutput &v) {
  a("output_ordinal", v.output_ordinal);
  a("properties_ref", v.properties_ref);
}
template <class A> void fields(A &a, LotPropertiesRef &v) {
  a("properties_id", v.properties_id);
}
template <class A> void fields(A &a, CargoSelection &v) {
  a("inventory_id", v.inventory_id);
  a("lot_id", v.lot_id);
  a("quantity", v.quantity);
}
template <class A> void fields(A &a, Reservation &v) {
  a("reference", v.action_id);
  a("amount", v.amount);
}
template <class A> void fields(A &a, MarketOriginalLotFlow &v) {
  tag(a, "kind", v.kind, 0,
      static_cast<Integer>(ResourceLotFlowDraft::Kind::selected_consume));
  a("source_inventory_id", v.source_inventory_id);
  a("source_lot_id", v.source_lot_id);
  a("destination_inventory_id", v.destination_inventory_id);
  a("destination_lot_id", v.destination_lot_id);
  a("item_id", v.item_id);
  a("quantity", v.quantity);
}
template <class A> void fields(A &a, OriginalBusinessItemOperation &v) {
  a("operation_index", v.operation_index);
  a("source_slot", v.source_slot);
  a("selections", v.selections);
  a("material_need", v.material_need);
  a("output_basis", v.output_basis);
  a("lot_flows", v.lot_flows);
  a.order(v.field_order);
}
template <class A> void fields(A &a, TradeWalletPosting &v) {
  a("wallet_id", v.wallet_id);
  a("owner", v.owner);
  a("delta_qian", v.delta_qian);
  a("balance_before", v.balance_before);
  a("balance_after", v.balance_after);
  a("reservations_before", v.reservations_before);
  a("reservations_after", v.reservations_after);
  a("spendable_before", v.spendable_before);
}
template <class A> void fields(A &a, OriginalBusinessStage &v) {
  tag(a, "stage", v.stage, 0, 3);
  a("next_instance_before", v.next_instance_before);
  a("next_instance_after", v.next_instance_after);
  a("inventory_frames", v.inventory_frames);
  a("item_operations", v.item_operations);
  a("wallet_postings", v.wallet_postings);
  a.order(v.field_order);
}
template <class A> void fields(A &a, OriginalBusinessFifoExecution &v) {
  a("schema_version", v.schema_version);
  a("adapter_rule_ref", v.adapter_rule_ref);
  a("time_basis", v.time_basis);
  a("stages", v.stages);
  a.order(v.field_order);
}
template <class A> void fields(A &a, MarketLotChange &v) {
  a("lot_id", v.lot_id);
  a("before", v.before);
  a("after", v.after);
  a.order(v.field_order);
}
template <class A> void fields(A &a, MarketInventoryChange &v) {
  a("inventory_id", v.inventory_id);
  a("revision_before", v.revision_before);
  a("revision_after", v.revision_after);
  a("lots", v.lots);
  a.order(v.field_order);
}
template <class A> void fields(A &a, MarketCaptureStep &v) {
  tag(a, "stage", v.stage, 0, 3);
  a("next_instance_before", v.next_instance_before);
  a("next_instance_after", v.next_instance_after);
  a("inventories", v.inventories);
  a("source_binding", v.source_binding);
  a.order(v.field_order);
}
template <class A> void fields(A &a, MarketMovementRecord &v) {
  a("kind", v.kind);
  a("business_fact_id", v.business_fact_id);
  tag(a, "business_kind", v.business_kind, 0, 2);
  a("business_source_schema", v.business_source_schema);
  a("source_tick", v.source_tick);
  a("observed_tick", v.observed_tick);
  a("steps", v.steps);
  a("original_business_execution", v.original_business_execution);
  a.order(v.field_order);
}
template <class A> void fields(A &a, GrowthMarketHistoryState &v) {
  a("version", v.version);
  a("movements", v.movements);
  a.order(v.field_order);
}
} // namespace hanmo::resident::growth_market_v2_fields
