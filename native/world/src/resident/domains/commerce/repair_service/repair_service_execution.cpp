#include "repair_service_internal.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
Status validate_repair_commercial_accepted_terms(const RepairServiceReadContext&x,const RepairAcceptedTerms&accepted,const WorldMoment&at,const RepairAcceptedSource*actual){try{
 const auto&t=contract(x,accepted.fee_terms_ref);const auto&v=t.terms;
 if(!same(accepted.return_terms_ref,accepted.fee_terms_ref)||v.instance_id!=accepted.instance_id||v.worker_id!=accepted.worker_id||v.repair_policy_ref.policy_id!=accepted.policy_ref.policy_id||v.repair_policy_ref.version!=accepted.policy_ref.version||!owner(v.customer,accepted.legal_owner)||!same(v.actual_contact_source,accepted.actual_contact_ref)||!same(v.owner_authority_source,accepted.owner_authority_ref)||v.terms_revision!=accepted.terms_revision||compare(t.accepted_at,at)||(v.custody_terms.delivery_before_acceptance&&!accepted.delivered_custody_source_ref))fail("RepairOriginalCommercialAcceptanceMismatch",accepted.instance_id);
 if(actual&&accepted_scope(x,t).id!=actual->id)fail("RepairOriginalCommercialAcceptanceReverseMismatch",actual->id);
 // The actual inspection version and original handover permission are still
 // independently checked by the original S9 acceptance provider.
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status consume_original_repair_start_fee_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const RepairJob&job){try{
 writable(c,i,e,x);const auto&t=contract(x,job.terms.fee_terms_ref);if(t.terms.fee_terms.trigger!=RepairServiceFeeTrigger::actual_start)return common_success();const auto source=take(charge_actual_repair_service_in_candidate(c,i,e,x,job.terms.fee_terms_ref,{repair_job_source(job.id),1}));take(pay_actual_repair_service_charge_in_candidate(c,i,e,x,{source,1}));return common_success();
}catch(Failure&error){return {false,{},std::move(error.error)};}}
Outcome<std::vector<MomentOwnerSource>>consume_original_repair_completion_fee_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const RepairJob&job,const RepairCompletedSource&done){try{
 writable(c,i,e,x);const auto&t=contract(x,job.terms.fee_terms_ref);if(done.job_id!=job.id||i.repair_jobs.completions.find(done.id)!=&done)fail("RepairFeeOriginalCompletionRequired",job.id);std::vector<MomentOwnerSource>result;
 if(t.terms.fee_terms.trigger==RepairServiceFeeTrigger::repair_completed){const auto source=take(charge_actual_repair_service_in_candidate(c,i,e,x,job.terms.fee_terms_ref,{repair_completed_source(done.id),1}));take(pay_actual_repair_service_charge_in_candidate(c,i,e,x,{source,1}));result.push_back({source,1});}return {true,std::move(result),{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
Status validate_original_repair_completion_fee(const RepairServiceReadContext&x,const RepairJob&job,const RepairCompletedSource&done){try{
 const auto&t=contract(x,job.terms.fee_terms_ref);if(done.job_id!=job.id||x.items.repair_jobs.completions.find(done.id)!=&done)fail("RepairFeeOriginalCompletionRequired",job.id);
 if(t.terms.fee_terms.trigger==RepairServiceFeeTrigger::actual_start){if(!done.actual_fee_sources.empty())fail("RepairUnexpectedCompletionFee",done.id);return common_success();}
 if(done.actual_fee_sources.size()!=1)fail("RepairCompletionFeeCoverageMismatch",done.id);const auto&q=charge(x,done.actual_fee_sources.front());if(!same(q.contract_ref,job.terms.fee_terms_ref)||!same(q.actual_trigger_source,{repair_completed_source(done.id),1}))fail("RepairCompletionFeeBindingMismatch",done.id);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
