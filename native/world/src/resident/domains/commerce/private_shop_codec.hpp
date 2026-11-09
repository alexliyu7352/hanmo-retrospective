// 商铺独立段严格逐字段保存；借用客户来源先冻结本域条款，旧经济和Growth段字节不动。
#pragma once
#include "private_shop.hpp"
#include "private_shop_codec3.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// PrivateShopPremises 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopPremises &v) {
  a("id", v.id);
  a("place_id", v.place_id);
  a("lessor_actor_id", v.lessor_actor_id);
  a("lessor_wallet_id", v.lessor_wallet_id);
  a("lessor_role_id", v.lessor_role_id);
  a("agreement_dialogue_id", v.agreement_dialogue_id);
  a("agreement_node_id", v.agreement_node_id);
  a("market_shop_id", v.market_shop_id);
  a("opening_fee", v.opening_fee);
  a("initial_capital", v.initial_capital);
  a("inventory_capacity", v.inventory_capacity);
  a("daily_rent", v.daily_rent);
  a("rent_rate_profile", v.rent_rate_profile);
}
// 店员独立条款与本人声明只归本私铺段；原Estate雇约和原LE字节不增字段。
template <class A> void record(A &a, PrivateShopStaffPolicy &v) {
  a("id", v.id);
  a("premises_id", v.premises_id);
  a("worker_actor_id", v.worker_actor_id);
  a("wage_rate_profile", v.wage_rate_profile);
  a("employer_dialogue_id", v.employer_dialogue_id);
  a("employer_offer_node_id", v.employer_offer_node_id);
  a("worker_dialogue_id", v.worker_dialogue_id);
  a("worker_accept_node_id", v.worker_accept_node_id);
  a("service_ticks", v.service_ticks);
  a("service_item_id", v.service_item_id);
}
template <class A> void record(A &a, PrivateShopStaffCatalog &v) {
  a("policies", v.policies);
}
// 订单目录只声明真实客户原say与冻结价款；受领期限不是钱款或库存余额。
template <class A> void record(A &a, PrivateShopOrderPolicy &v) {
  a("id", v.id);
  a("premises_id", v.premises_id);
  a("client_actor_id", v.client_actor_id);
  a("client_role_id", v.client_role_id);
  a("dialogue_id", v.dialogue_id);
  a("node_id", v.node_id);
  a("task_template_id", v.task_template_id);
  a("item_id", v.item_id);
  a("quantity", v.quantity);
  a("contract_price", v.contract_price);
  a("accept_ticks", v.accept_ticks);
  a("deadline_ticks", v.deadline_ticks);
}
template <class A> void record(A &a, PrivateShopOrderCatalog &v) {
  a("policies", v.policies);
}
// PrivateShopCatalog 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopCatalog &v) {
  a("version", v.version);
  a("premises", v.premises);
  a("staff", v.staff);
  a("orders", v.orders);
}
// PrivateShopOpeningTerms 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopOpeningTerms &v) {
  a("premises_id", v.premises_id);
  a("place_id", v.place_id);
  a("proprietor_actor_id", v.proprietor_actor_id);
  a("lessor_actor_id", v.lessor_actor_id);
  a("payer_wallet_id", v.payer_wallet_id);
  a("lessor_wallet_id", v.lessor_wallet_id);
  a("market_shop_id", v.market_shop_id);
  a("opening_fee", v.opening_fee);
  a("initial_capital", v.initial_capital);
  a("inventory_capacity", v.inventory_capacity);
  a("daily_rent", v.daily_rent);
  a("rent_rate_profile", v.rent_rate_profile);
}
// PrivateShopMeetingSource 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopMeetingSource &v) {
  a("actor_id", v.actor_id);
  a("provider_actor_id", v.provider_actor_id);
  a("session_id", v.session_id);
  a("dialogue_id", v.dialogue_id);
  a("node_id", v.node_id);
  a("place_id", v.place_id);
  a("role_id", v.role_id);
}
// PrivateShopClientQualification 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopClientQualification &v) {
  a("original_client", v.original_client);
  a("delivery_beneficiary", v.delivery_beneficiary);
  a("task_id", v.task_id);
  a("settlement_id", v.settlement_id);
  a("template_id", v.template_id);
  a("payment_fact_id", v.payment_fact_id);
  a("completed_tick", v.completed_tick);
  a("objective_fact_ids", v.objective_fact_ids);
}
// PrivateShopQualification 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopQualification &v) {
  a("minimum_commerce_xp", v.minimum_commerce_xp);
  a("minimum_commerce_level", v.minimum_commerce_level);
  a("commerce_study_source_ids", v.commerce_study_source_ids);
  a("original_clients", v.original_clients);
  a("permit_fact_id", v.permit_fact_id);
  a("clearance_fact_id", v.clearance_fact_id);
}
// PrivateShopRecord 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopRecord &v) {
  a("id", v.id);
  a("status", v.status);
  a("wallet_id", v.wallet_id);
  a("inventory_id", v.inventory_id);
  a("rent_obligation_id", v.rent_obligation_id);
  a("terms", v.terms);
  a("meeting", v.meeting);
  a("qualification", v.qualification);
  a("opened_tick", v.opened_tick);
  a("markup_basis_points", v.markup_basis_points);
}
// PrivateShopCapitalFact 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopCapitalFact &v) {
  a("id", v.id);
  a("shop_id", v.shop_id);
  a("actor_id", v.actor_id);
  a("from_wallet_id", v.from_wallet_id);
  a("to_wallet_id", v.to_wallet_id);
  a("tick", v.tick);
  a("amount", v.amount);
}
// PrivateShopMarkupFact 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopMarkupFact &v) {
  a("id", v.id);
  a("shop_id", v.shop_id);
  a("actor_id", v.actor_id);
  a("tick", v.tick);
  a("before_basis_points", v.before_basis_points);
  a("after_basis_points", v.after_basis_points);
}
// 采购原批次及实际入店新批次均保稳定ID，不用今日库存代替购入成本。
template <class A> void record(A &a, PrivateShopStockLot &v) {
  a("lot_id", v.lot_id);
  a("quantity", v.quantity);
}
template <class A> void record(A &a, PrivateShopPurchaseFact &v) {
  a("id", v.id);
  a("shop_id", v.shop_id);
  a("trade_fact_id", v.trade_fact_id);
  a("item_id", v.item_id);
  a("market_shop_id", v.market_shop_id);
  a("from_inventory_id", v.from_inventory_id);
  a("to_inventory_id", v.to_inventory_id);
  a("tick", v.tick);
  a("quantity", v.quantity);
  a("purchase_cash", v.purchase_cash);
  a("purchased_lots", v.purchased_lots);
  a("stocked_lots", v.stocked_lots);
}
// 原Action与每个真实到岗边界同序保存，冷读不能把日历经过推成出勤。
template <class A> void record(A &a, PrivateShopOperation &v) {
  a("id", v.id);
  a("shop_id", v.shop_id);
  a("actor_id", v.actor_id);
  a("item_id", v.item_id);
  a("rent_arrear_id", v.rent_arrear_id);
  a("rent_payment_id", v.rent_payment_id);
  a("result_fact_id", v.result_fact_id);
  a("staff_contract_id", v.staff_contract_id);
  a("start_tick", v.start_tick);
  a("markup_basis_points", v.markup_basis_points);
  a("attendance_ticks", v.attendance_ticks);
}
template <class A> void record(A &a, PrivateShopRetailLot &v) {
  a("purchase_fact_id", v.purchase_fact_id);
  a("lot_id", v.lot_id);
  a("quantity", v.quantity);
  a("cost", v.cost);
}
template <class A> void record(A &a, PrivateShopDayResult &v) {
  a("id", v.id);
  a("operation_id", v.operation_id);
  a("shop_id", v.shop_id);
  a("actor_id", v.actor_id);
  a("item_id", v.item_id);
  a("reason", v.reason);
  a("tick", v.tick);
  a("market_observed_tick", v.market_observed_tick);
  a("market_revision", v.market_revision);
  a("market_stock", v.market_stock);
  a("base_price", v.base_price);
  a("midpoint", v.midpoint);
  a("quantity", v.quantity);
  a("income", v.income);
  a("cost_of_goods", v.cost_of_goods);
  a("rent_paid", v.rent_paid);
  a("wage_paid", v.wage_paid);
  a("net_income", v.net_income);
  a("completed", v.completed);
  a("consumed_lots", v.consumed_lots);
}
// 雇用仅存本人合同/终止事实，薪资仍从原LE应付、付款和余数证明。
template <class A> void record(A &a, PrivateShopStaffTerms &v) {
  a("policy_id", v.policy_id);
  a("shop_id", v.shop_id);
  a("employer_actor_id", v.employer_actor_id);
  a("worker_actor_id", v.worker_actor_id);
  a("payer_wallet_id", v.payer_wallet_id);
  a("payee_wallet_id", v.payee_wallet_id);
  a("rate_profile", v.rate_profile);
  a("wage_numerator", v.wage_numerator);
  a("wage_denominator", v.wage_denominator);
  a("service_ticks", v.service_ticks);
  a("service_item_id", v.service_item_id);
}
template <class A> void record(A &a, PrivateShopStaffContract &v) {
  a("id", v.id);
  a("status", v.status);
  a("terminal_fact_id", v.terminal_fact_id);
  a("obligation_id", v.obligation_id);
  a("terms", v.terms);
  a("agreement", v.agreement);
  a("accepted_tick", v.accepted_tick);
  a("through_tick", v.through_tick);
  a("accepted_by_worker", v.accepted_by_worker);
}
template <class A> void record(A &a, PrivateShopStaffTermination &v) {
  a("id", v.id);
  a("contract_id", v.contract_id);
  a("actor_id", v.actor_id);
  a("decision", v.decision);
  a("tick", v.tick);
  a("cause_fact_id", v.cause_fact_id);
}
template <class A> void record(A &a, PrivateShopStaffService &v) {
  a("id", v.id);
  a("contract_id", v.contract_id);
  a("operation_id", v.operation_id);
  a("arrear_id", v.arrear_id);
  a("payment_id", v.payment_id);
  a("tick", v.tick);
  a("attendance_index", v.attendance_index);
  a("amount", v.amount);
  a("remainder_before", v.remainder_before);
  a("remainder_after", v.remainder_after);
  a("closing_fraction", v.closing_fraction);
}
template <class A> void record(A &a, PrivateShopStaffState &v) {
  a("contracts", v.contracts);
  a("decisions", v.decisions);
  a("services", v.services);
}
// 订单与原Task一次绑定独立逐字段；原Task visitor及双方Actor默认容器保持。
template <class A> void record(A &a, PrivateShopOrderTerms &v) {
  a("order_id", v.order_id);
  a("shop_id", v.shop_id);
  a("proprietor_actor_id", v.proprietor_actor_id);
  a("original_client", v.original_client);
  a("task_template_id", v.task_template_id);
  a("item_id", v.item_id);
  a("stock_inventory_id", v.stock_inventory_id);
  a("shop_wallet_id", v.shop_wallet_id);
  a("client_inventory_id", v.client_inventory_id);
  a("client_wallet_id", v.client_wallet_id);
  a("quantity", v.quantity);
  a("contract_price", v.contract_price);
  a("accepted_tick", v.accepted_tick);
  a("deadline_tick", v.deadline_tick);
}
template <class A> void record(A &a, PrivateShopOrderRecord &v) {
  a("id", v.id);
  a("policy_id", v.policy_id);
  a("status", v.status);
  a("task_id", v.task_id);
  a("acceptance_fact_id", v.acceptance_fact_id);
  a("binding_fact_id", v.binding_fact_id);
  a("terms", v.terms);
  a("client_declaration", v.client_declaration);
  a("issued_tick", v.issued_tick);
  a("valid_through_tick", v.valid_through_tick);
}
template <class A> void record(A &a, PrivateShopOrderBinding &v) {
  a("id", v.id);
  a("order_id", v.order_id);
  a("task_id", v.task_id);
  a("acceptance_fact_id", v.acceptance_fact_id);
  a("tick", v.tick);
}
template <class A> void record(A &a, PrivateShopOrderState &v) {
  a("orders", v.orders);
  a("bindings", v.bindings);
}
// 订单成交保存唯一Task结算及原批次腿，未卖成本不会进入损益。
template <class A> void record(A &a, PrivateShopOrderLotTransfer &v) {
  a("original_lot_id", v.original_lot_id);
  a("destination_lot_id", v.destination_lot_id);
  a("quantity", v.quantity);
}
template <class A> void record(A &a, PrivateShopOrderSettlement &v) {
  a("id", v.id);
  a("order_id", v.order_id);
  a("task_id", v.task_id);
  a("task_settlement_id", v.task_settlement_id);
  a("receipt_fact_id", v.receipt_fact_id);
  a("outcome", v.outcome);
  a("tick", v.tick);
  a("income", v.income);
  a("cost_of_goods", v.cost_of_goods);
  a("delivered_lots", v.delivered_lots);
  a("consumed_lots", v.consumed_lots);
}
// 原关闭收据逐批保留实际归还腿；已售成本和本人本金不改名为退款利润。
template <class A> void record(A &a, PrivateShopClosingLot &v) {
  a("original_lot_id", v.original_lot_id);
  a("destination_lot_id", v.destination_lot_id);
  a("item_id", v.item_id);
  a("quantity", v.quantity);
}
template <class A> void record(A &a, PrivateShopClosingRecord &v) {
  a("id", v.id);
  a("shop_id", v.shop_id);
  a("actor_id", v.actor_id);
  a("from_wallet_id", v.from_wallet_id);
  a("to_wallet_id", v.to_wallet_id);
  a("from_inventory_id", v.from_inventory_id);
  a("to_inventory_id", v.to_inventory_id);
  a("tick", v.tick);
  a("returned_cash", v.returned_cash);
  a("returned_lots", v.returned_lots);
}
// PrivateShopState 的具名持久字段供JSON与二进制同序遍历。
template <class A> void record(A &a, PrivateShopState &v) {
  a("version", v.version);
  a("initialized_tick", v.initialized_tick);
  a("last_step_tick", v.last_step_tick);
  private_shop_records(a, v.shops, v.version);
  a("capital", v.capital);
  a("markups", v.markups);
  a("purchases", v.purchases);
  a("operations", v.operations);
  a("operating_results", v.operating_results);
  a("staff", v.staff);
  a("orders", v.orders);
  a("order_settlements", v.order_settlements);
  a("closings", v.closings);
}
} // namespace hanmo::resident::dynamic_economy_codec
