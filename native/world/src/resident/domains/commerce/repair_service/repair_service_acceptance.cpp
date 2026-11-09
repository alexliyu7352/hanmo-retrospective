#include "repair_service_internal.hpp"
#include "resident/domains/craft/repair_acceptance.hpp"
#include "resident/domains/craft/repair_codec.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
Outcome<RepairServiceAcceptedResult>accept_original_repair_service_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairServiceReadContext&x,const RepairReadContext&repair,const RepairServiceContractTerms&t,const RepairServiceAcceptanceInspection&inspection){try{
 writable(c,i,x);if(x.repair!=&repair||repair.commercial_sources!=&x)fail("RepairAcceptanceTwoOwnerContextMismatch",t.instance_id);
 const auto commercial=take(accept_repair_service_contract_in_candidate(c,i,x,t));const MomentOwnerSource contract_ref{commercial,1};
 RepairAcceptedTerms accepted;accepted.policy_ref=t.repair_policy_ref;accepted.instance_id=t.instance_id;accepted.worker_id=t.worker_id;accepted.original_item=inspection.original_item;accepted.legal_owner=t.customer;accepted.condition_head=inspection.condition_head;accepted.workstation=inspection.workstation;accepted.actual_contact_ref=t.actual_contact_source;accepted.owner_authority_ref=t.owner_authority_source;accepted.fee_terms_ref=contract_ref;accepted.return_terms_ref=contract_ref;accepted.delivered_custody_source_ref=inspection.delivered_custody_source_ref;accepted.terms_revision=t.terms_revision;
 for(const auto&row:i.repair_jobs.acceptances.rows())if(same(row.second.terms.fee_terms_ref,contract_ref)){
  if(!same_repair_accepted_terms(row.second.terms,accepted))fail("ConflictingRepairInspectionReplay",row.first);const MomentOwnerSource original{repair_accepted_source(row.first),1};take(read_repair_accepted_service(repair,original));take(validate_repair_service_contracts(x));return {true,{contract_ref,original},{}};
 }
 const auto quote=take(preview_repair_acceptance(repair,accepted));const auto actual=take(accept_repair_service_in_candidate(c,i,repair,quote));take(finalize_repair_service_acceptance_in_candidate(c,i,x,contract_ref));take(validate_repair_acceptances(repair));return {true,{contract_ref,{actual,1}},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status consume_original_repair_termination_finance_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const MomentOwnerSource&r,const MomentOwnerSource&terminal){try{
 writable(c,i,e,x);for(const auto&row:state(x).termination_settlements.rows())if(same(row.second.contract_ref,r)){const auto&old=settlement(x,{repair_service_termination_settled_source(row.first),1});if(!same(old.actual_terminal_ref,terminal))fail("ConflictingRepairSettlementReplay",row.first);take(validate_repair_service_contracts(x));return common_success();}
 const auto source=take(settle_original_repair_service_termination_in_candidate(c,i,e,x,r,terminal));settlement(x,{source,1});take(validate_repair_service_contracts(x));return common_success();
}catch(Failure&error){return {false,{},std::move(error.error)};}}
Outcome<SourceRef>interrupt_unstarted_repair_service_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const RepairReadContext&repair,const MomentOwnerSource&accepted,const MomentOwnerSource&cause){try{
 writable(c,i,e,x);if(x.repair!=&repair||repair.commercial_sources!=&x)fail("RepairTerminationTwoOwnerContextMismatch",accepted.source_ref.id);const auto prior=take(read_repair_accepted_service(repair,accepted));const auto contract_ref=prior.acceptance->terms.fee_terms_ref;
 const auto terminal=take(interrupt_unstarted_repair_in_candidate(c,i,repair,accepted,cause));take(consume_original_repair_termination_finance_in_candidate(c,i,e,x,contract_ref,{terminal,1}));return {true,terminal,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
}
