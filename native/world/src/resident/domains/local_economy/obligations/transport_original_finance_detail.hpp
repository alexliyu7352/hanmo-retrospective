// Internal original transport validation, shared by the real writer's scoped
// adoption and the stable P11 reader. No switch can relax a published reader.
#pragma once
#include "transport_original_obligations.hpp"
namespace hanmo::resident::transport_finance_detail {
struct Failure {RuleError error;};
[[noreturn]]inline void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
inline void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
inline int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
inline bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
inline SourceRef adoption(const TransportPaymentReceipt&r){return {"local_economy","transport_expense_paid",r.id};}
inline SourceRef arrear(const TransportPaymentReceipt&r){return {"local_economy","contract_payment_due",r.original_arrear_id};}
inline SourceRef payment(const TransportPaymentReceipt&r){return {"local_economy","obligation_payment",r.original_payment_receipt_id};}
inline MomentOwnerSource expense(const TransportPaymentReceipt&r){return {r.expense.source_ref,r.expense.source_schema};}
std::size_t header(const CommonStore&,const SourceRef&);
bool same_cost(const ActualTransportExpense&,const ActualTransportExpense&);
struct OriginalTransportFinance {OriginalEconomicPayable payable;ObligationResponsibilityEpoch genesis;};
OriginalTransportFinance original(const CommonStore&,const TransportPaymentReceipt&,
 const ActualTransportExpense&,const TransportExpenseResponsibility&);
OriginalTransportFinance historical(const CommonStore&,const TransportPaymentReceipt&,
 const EconomicTransportPaymentContext&);
OriginalEconomicCashPayment cash(const CommonStore&,const LocalEconomyState&,
 const TransportPaymentReceipt&);
const TransportPaymentReceipt* by_payment(const EconomicTransportExpenseState&,const SourceRef&);
}
