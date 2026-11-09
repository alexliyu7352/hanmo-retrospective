#include "repair_service_financial_sources.hpp"
#include "repair_service_internal.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
Status visit_original_repair_service_financial_sources(const RepairServiceReadContext&x,const MomentOwnerSource&accepted,RepairServiceFinancialSourceVisitor&sink){try{
 if(!x.repair)fail("RepairFinancialCanonicalScopeReaderRequired",accepted.source_ref.id,ErrorKind::unsupported_capability);
 const auto scope=take(read_repair_accepted_service(*x.repair,accepted));const auto&t=contract(x,scope.acceptance->terms.fee_terms_ref);if(accepted_scope(x,t).id!=scope.acceptance->id)fail("RepairFinancialCanonicalScopeMismatch",accepted.source_ref.id);
 take(validate_repair_service_contracts(x));const MomentOwnerSource original{repair_service_contract_source(t.id),1};take(sink.contract(t,*scope.acceptance));const auto&s=state(x);
 for(const auto&row:s.charges.rows())if(same(row.second.contract_ref,original))take(sink.charge(row.second));
 for(const auto&row:s.payments.rows()){const auto&q=charge(x,row.second.charge_ref);if(same(q.contract_ref,original)){const auto*actual=x.economy.payments.find(row.second.actual_payment_ref.id);if(!actual)fail("RepairOriginalPaymentRequired",row.first);take(sink.payment(row.second,*actual));}}
 for(const auto&row:s.termination_settlements.rows())if(same(row.second.contract_ref,original))take(sink.settlement(row.second));
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
