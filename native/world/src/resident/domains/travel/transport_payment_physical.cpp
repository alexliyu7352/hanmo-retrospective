#include "transport_payment_physical.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
void actual_header(const CommonStore& common,const SourceRef& ref){const auto* head=common.state().facts.find(ref.id);if(!head||head->domain!=ref.domain||head->kind!=ref.kind)fail("TransportPaymentPhysicalActualHeaderRequired",ref.id);}
bool is_receipt(const SourceRef& ref){return ref.domain=="local_economy"&&ref.kind=="transport_expense_paid";}
}
const TransportPaymentPhysicalBinding* TransportPaymentPhysicalIndex::find_payment(const StableId& id)const noexcept{const auto it=std::lower_bound(bindings.begin(),bindings.end(),id,[](const auto& x,const auto& key){return x.payment->id<key;});return it!=bindings.end()&&it->payment->id==id?&*it:nullptr;}
Outcome<TransportPaymentPhysicalIndex> index_transport_payment_physical_owners(const CommonStore& common,const MovementExactState& movement,const SpatialCarrierState* carriers,const GroupMovementExecutionState* executions,const EconomicTransportExpenseState& payments,ExactArithmeticBudget budget){return guard<TransportPaymentPhysicalIndex>([&]{
 TransportPaymentPhysicalIndex result;
 for(const auto& payment:payments.payments){
  if(payment.id.empty()||!source_valid(payment.request.actual_movement_or_handling_source)||payment.request.source_schema<1||payment.request.source_schema!=payment.expense.source_schema||!same_source(payment.request.actual_movement_or_handling_source,payment.expense.source_ref))fail("TransportPaymentPhysicalSourceMismatch",payment.id);
  actual_header(common,{"local_economy","transport_expense_paid",payment.id});
  const auto it=std::lower_bound(result.bindings.begin(),result.bindings.end(),payment.id,[](const auto& x,const auto& key){return x.payment->id<key;});
  if(it!=result.bindings.end()&&it->payment->id==payment.id)fail("DuplicateTransportPayment",payment.id);
  result.bindings.insert(it,{&payment,TransportPaymentPhysicalOwner::movement_unit,nullptr});
 }
 auto adopt=[&](const SourceRef& receipt,TransportPaymentPhysicalOwner owner,const StableId& owner_id)->const TransportPaymentReceipt&{
  if(!is_receipt(receipt))fail("TransportPaymentPhysicalReceiptRefRequired",receipt.id);
  auto* binding=const_cast<TransportPaymentPhysicalBinding*>(result.find_payment(receipt.id));
  if(!binding)fail("TransportPaymentPhysicalReceiptRequired",receipt.id);
  if(binding->owner_id)fail("TransportPaymentMultiplePhysicalOwners",receipt.id);
  binding->owner=owner;binding->owner_id=&owner_id;return *binding->payment;
 };
 if(bool(carriers)!=bool(executions))fail("TransportPaymentGroupRootsNotPaired");
 if(executions)for(const auto& row:executions->expenses.rows()){
  const auto& source=row.second;
  if(!source.original_payment_receipt)continue; // The original Army single debit has no S5 receipt.
  if(row.first!=source.id)fail("GroupExpenseSourceKeyMismatch",row.first);
  const auto* unit=movement.units.find(source.unit_ref);const auto* journey=carriers->journeys.find(source.journey_id);
  const auto* paid=unit?std::get_if<MovementPaymentConsumed>(&unit->payment):nullptr;
  const auto* movement_cost=paid?movement.expenses.find(paid->actual_expense_source.id):nullptr;
  if(!unit||!journey||!paid||!movement_cost||!std::holds_alternative<TransportExpenseAuthority>(journey->expense_authority)||paid->actual_expense_source.domain!="travel"||paid->actual_expense_source.kind!="movement_expense"||movement_cost->unit_ref!=unit->id||movement_cost->prefix_id!=source.prefix_ref||execution_key(movement_cost->execution)!=execution_key(unit->execution))fail("TransportPaymentPhysicalUnitRequired",source.id);
  actual_header(common,{"travel","group_transport_expense",source.id});
  const auto& payment=adopt(*source.original_payment_receipt,TransportPaymentPhysicalOwner::movement_unit,unit->id);
  if(!same_source(payment.expense.source_ref,{"travel","group_transport_expense",source.id})||payment.expense.source_schema!=1||payment.expense.segment_or_handling_id!=unit->id||payment.expense.journey_id!=journey->journey_id||payment.payer_wallet_id!=paid->payer_wallet_id||payment.expense.amount!=paid->amount||movement_cost->amount!=paid->amount||!same_moment(payment.expense.occurred_at,source.occurred_at,budget)||!same_moment(movement_cost->occurred_at,source.occurred_at,budget))fail("TransportPaymentPhysicalUnitSourceMismatch",payment.id);
 }
 if(carriers)for(const auto& row:carriers->crossings.rows()){
  const auto& crossing=row.second;if(row.first!=crossing.receipt_id)fail("BoundaryCrossingKeyMismatch",row.first);
  for(const auto& ref:crossing.payment_refs){
   const auto& payment=adopt(ref,TransportPaymentPhysicalOwner::boundary_crossing,crossing.receipt_id);
   if(payment.expense.purpose!=TransportExpensePurpose::toll||!same_moment(payment.expense.occurred_at,crossing.entry_at,budget))fail("TransportPaymentPhysicalTollSourceMismatch",payment.id);
  }
 }
 auto handling_payment=[&](const SourceRef& source)->const TransportPaymentReceipt*{
  if(is_receipt(source)){const auto* row=result.find_payment(source.id);if(!row)fail("CargoHandlingPaymentReceiptRequired",source.id);return row->payment;}
  const TransportPaymentReceipt* found=nullptr;
  for(const auto& row:result.bindings)if(same_source(row.payment->request.actual_movement_or_handling_source,source)){if(found)fail("DuplicateCargoHandlingCostPayment",source.id);found=row.payment;}
  // A business may retain a real nonpayment cost source. It owns no S5 row.
  return found;
 };
 for(const auto& row:movement.cargo_handling.terms.rows()){
  const auto& terms=row.second;if(row.first!=terms.terms_id)fail("CargoHandlingTermsKeyMismatch",row.first);
  for(const auto& ref:terms.actual_start_cost_sources){const auto* found=handling_payment(ref);if(!found)continue;
   const auto& payment=adopt({"local_economy","transport_expense_paid",found->id},TransportPaymentPhysicalOwner::handling_terms,terms.terms_id);
   if(payment.expense.purpose!=TransportExpensePurpose::handling||!same_moment(payment.expense.occurred_at,terms.accepted_at,budget))fail("CargoHandlingStartPaymentSourceMismatch",payment.id);
  }
 }
 for(const auto& row:movement.cargo_handling.work_results.rows()){
  const auto& work=row.second;const auto* terms=movement.cargo_handling.terms.find(work.terms_ref);const auto* execution=movement.cargo_handling.executions.find(work.execution_ref);
  if(row.first!=work.result_id||!terms||!execution||execution->terms_ref!=terms->terms_id||execution->work_result_ref!=std::optional<StableId>{work.result_id})fail("CargoHandlingWorkPaymentOwnerRequired",row.first);
  for(const auto& ref:work.cost_source_refs){const auto* found=handling_payment(ref);if(!found)continue;
   const auto* existing=result.find_payment(found->id);
   if(existing->owner_id){
    if(existing->owner!=TransportPaymentPhysicalOwner::handling_terms||*existing->owner_id!=terms->terms_id)fail("CargoHandlingPaymentMultipleOwners",found->id);
    bool actual_start=false;for(const auto& start:terms->actual_start_cost_sources){const auto* payment=handling_payment(start);if(payment&&payment->id==found->id)actual_start=true;}
    if(!actual_start)fail("CargoHandlingStartPaymentRefRequired",found->id);
    continue; // Reference only: the Terms row already owns the complete receipt.
   }
   const auto& payment=adopt({"local_economy","transport_expense_paid",found->id},TransportPaymentPhysicalOwner::handling_work_result,work.result_id);
   if(payment.expense.purpose!=TransportExpensePurpose::handling||!same_moment(payment.expense.occurred_at,work.service_completed_at,budget))fail("CargoHandlingCompletionPaymentSourceMismatch",payment.id);
  }
 }
 for(const auto& row:result.bindings)if(!row.owner_id)fail("TransportPaymentPhysicalOwnerMissing",row.payment->id);
 return result;
});}
}
