// 原运输付款唯一字段表，外层嵌入必须保持原Authority/Fraction/enum adapter。
#pragma once
#include "economic_transport_sources.hpp"
namespace hanmo::resident::economic_transport_codec {
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,WorldMoment& v){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
template<class A>void record(A& a,TransportExpenseRequest& v){a("actual_movement_or_handling_source",v.actual_movement_or_handling_source);a("source_schema",v.source_schema);a("authority",v.authority);}
template<class A>void record(A& a,ActualTransportExpense& v){a("source_ref",v.source_ref);a("source_schema",v.source_schema);a("journey_id",v.journey_id);a("segment_or_handling_id",v.segment_or_handling_id);a("cost_rule_id",v.cost_rule_id);a("payee_wallet_id",v.payee_wallet_id);a("destination_id",v.destination_id);a("purpose",v.purpose);a("amount",v.amount);a("occurred_at",v.occurred_at);}
template<class A>void record(A& a,TransportPaymentReceipt& v){a("id",v.id);a("request",v.request);a("expense",v.expense);a("payer_wallet_id",v.payer_wallet_id);a("original_obligation_id",v.original_obligation_id);a("original_arrear_id",v.original_arrear_id);a("original_payment_receipt_id",v.original_payment_receipt_id);}
template<class A>void record(A& a,EconomicTransportExpenseState& v){a("payments",v.payments);}
}
