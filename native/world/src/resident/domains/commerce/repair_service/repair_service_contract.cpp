#include "repair_service_internal.hpp"
#include "repair_service_rules.hpp"
namespace hanmo::resident {
#define REPAIR_SOURCE(fn,kind) SourceRef fn(const StableId&id){return {"commerce",kind,id};}
REPAIR_SOURCE(repair_service_offer_source,"repair_service_offered")
REPAIR_SOURCE(repair_service_assent_source,"repair_service_assented")
REPAIR_SOURCE(repair_service_contract_source,"repair_service_contract_accepted")
REPAIR_SOURCE(repair_custody_received_source,"repair_custody_received")
REPAIR_SOURCE(repair_service_charge_source,"repair_service_charge")
REPAIR_SOURCE(repair_service_paid_source,"repair_service_paid")
REPAIR_SOURCE(repair_service_termination_settled_source,"repair_service_termination_settled")
REPAIR_SOURCE(repair_item_returned_source,"repair_item_returned")
#undef REPAIR_SOURCE
}
namespace hanmo::resident::repair_service_detail {
void contract_shape(const RepairServiceReadContext&x,const RepairServiceContractTerms&t,const WorldMoment&at,const RepairServiceContractSource*actual){
 const auto*p=take(read_repair_policy(x.policies,t.repair_policy_ref));take(compare_world_moments(at,at));
 if(t.instance_id.empty()||t.worker_id.empty()||!x.common.actor(t.worker_id)||!x.common.valid_resource_owner(t.customer)||!x.common.valid_resource_owner(t.provider)||t.customer.extra||t.provider.extra||t.payer_wallet_id.empty()||t.payee_wallet_id.empty()||t.payer_wallet_id==t.payee_wallet_id||t.terms_revision<1||compare(at,now(x))>0)fail("RepairServiceContractShapeMismatch",t.instance_id);
 const auto*item=x.items.instances.find(t.instance_id);if(!item||item->repair_policy_id!=p->policy_id||std::find(p->applicable_definitions.begin(),p->applicable_definitions.end(),item->item_definition_id)==p->applicable_definitions.end())fail("RepairServiceOriginalItemPolicyMismatch",t.instance_id);
 const auto&fee=t.fee_terms;if(fee.line_id.empty()||fee.amount_qian<0||fee.amount_qian!=p->fee_qian||(fee.trigger!=RepairServiceFeeTrigger::actual_start&&fee.trigger!=RepairServiceFeeTrigger::repair_completed))fail("RepairServiceFixedFeePolicyMismatch",t.instance_id);
 for(const auto*r:{&t.fee_policy_source,&fee.original_rule_ref,&t.actual_contact_source,&t.customer_acceptance_source,&t.provider_acceptance_source,&t.owner_authority_source,&t.payer_authority_source,&t.return_terms.receiver_authority,&t.return_terms.original_rule_ref,&t.custody_terms.authority_ref,&t.custody_terms.original_rule_ref})ref(*r);
 const auto&ret=t.return_terms;const auto&custody=t.custody_terms;
 if(!x.common.valid_resource_owner(ret.receiver)||ret.receiver.extra||!valid_source_ref(ret.site_ref)||custody.inventory_id.empty()||custody.uncollected_fee_qian<0||custody.uncollected_period_half_days<0||(custody.uncollected_fee_qian>0&&custody.uncollected_period_half_days<1))fail("RepairServiceReturnCustodyTermsMismatch",t.instance_id);
 take(compare_world_moments(ret.agreed_return_at,ret.agreed_return_at));take(compare_world_moments(custody.collection_due_at,custody.collection_due_at));if(compare(ret.agreed_return_at,at)<0||compare(custody.collection_due_at,ret.agreed_return_at)<0)fail("RepairServiceAgreedTimesMismatch",t.instance_id);
 // Complete native author rule validation precedes the actual assents. In
 // particular, these refs cannot approve an absent time-price/custody handler.
 if(!x.rules)fail("RepairOriginalCommercialRuleCatalogRequired",t.instance_id,ErrorKind::unsupported_capability);take(validate_repair_service_rule_terms(*x.rules,x.policies,t));
 take(x.original.validate_original_repair_service_rules(t,*p));
 if(actual){const auto cut=header(x.common,repair_service_contract_source(actual->id));for(const auto*r:{&t.actual_contact_source,&t.customer_acceptance_source,&t.provider_acceptance_source})if(header(x.common,r->source_ref)>=cut)fail("RepairServiceAcceptanceOrderMismatch",actual->id);}
 else for(const auto*r:{&t.actual_contact_source,&t.customer_acceptance_source,&t.provider_acceptance_source})header(x.common,r->source_ref);
 take(x.original.validate_original_repair_service_acceptance(t,at,actual));
}
const RepairServiceContractSource&contract(const RepairServiceReadContext&x,const MomentOwnerSource&r){
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_contract_source(r.source_ref.id)))fail("UnsupportedRepairServiceContract",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*t=state(x).contracts.find(r.source_ref.id);if(!t||t->id!=r.source_ref.id)fail("RepairServiceContractSourceRequired",r.source_ref.id);header(x.common,r.source_ref);contract_shape(x,t->terms,t->accepted_at,t);return *t;
}
const RepairAcceptedSource&accepted_scope(const RepairServiceReadContext&x,const RepairServiceContractSource&t){
 const RepairAcceptedSource*out=nullptr;const MomentOwnerSource original{repair_service_contract_source(t.id),1};
 for(const auto&row:x.items.repair_jobs.acceptances.rows())if(same(row.second.terms.fee_terms_ref,original)||same(row.second.terms.return_terms_ref,original)){
  const auto&a=row.second;const auto&v=a.terms;if(out||row.first!=a.id||!same(v.fee_terms_ref,original)||!same(v.return_terms_ref,original)||v.instance_id!=t.terms.instance_id||v.worker_id!=t.terms.worker_id||v.policy_ref.policy_id!=t.terms.repair_policy_ref.policy_id||v.policy_ref.version!=t.terms.repair_policy_ref.version||!owner(v.legal_owner,t.terms.customer)||!same(v.actual_contact_ref,t.terms.actual_contact_source)||!same(v.owner_authority_ref,t.terms.owner_authority_source)||v.terms_revision!=t.terms.terms_revision||compare(a.accepted_at,t.accepted_at)||header(x.common,repair_accepted_source(a.id))<=header(x.common,original.source_ref))fail("RepairServiceS9AcceptanceBindingMismatch",t.id);
  if(t.terms.custody_terms.delivery_before_acceptance&&!v.delivered_custody_source_ref)fail("RepairServiceRequiredEarlyCustodyMissing",t.id);out=&a;
 }
 if(!out)fail("RepairServiceS9AcceptanceRequired",t.id);return *out;
}
WorldMoment actual_trigger(const RepairServiceReadContext&x,const RepairServiceContractSource&t,const MomentOwnerSource&r){
 const auto&a=accepted_scope(x,t);if(!x.repair)fail("RepairServiceS9OriginalReaderRequired",t.id,ErrorKind::unsupported_capability);
 const RepairJob*j=nullptr;WorldMoment at;
 if(t.terms.fee_terms.trigger==RepairServiceFeeTrigger::actual_start){if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_job_source(r.source_ref.id)))fail("RepairServiceActualStartRequired",r.source_ref.id);j=take(read_original_repair_job(*x.repair,r.source_ref.id));at=j->started_at;}
 else {auto evidence=take(read_repair_completion_for_financial_evidence(*x.repair,r));j=evidence.job;at=evidence.completion->occurred_at;}
 if(!j||!same(j->terms.accepted_service_ref,{repair_accepted_source(a.id),1})||j->terms.instance_id!=t.terms.instance_id||!same(j->terms.fee_terms_ref,{repair_service_contract_source(t.id),1})||header(x.common,r.source_ref)<=header(x.common,repair_accepted_source(a.id))||compare(at,t.accepted_at)<0||compare(at,now(x))>0)fail("RepairServiceOriginalTriggerBindingMismatch",t.id);return at;
}

}
namespace hanmo::resident {using namespace repair_service_detail;
Outcome<const RepairServiceContractSource*>read_repair_service_contract(const RepairServiceReadContext&x,const MomentOwnerSource&r){try{return {true,&contract(x,r),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Status validate_repair_service_contract_proposal(const RepairServiceReadContext&x,const RepairServiceContractTerms&t){try{context(x);contract_shape(x,t,now(x),nullptr);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<SourceRef>accept_repair_service_contract_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairServiceReadContext&x,const RepairServiceContractTerms&t){try{
 writable(c,i,x);auto&s=i.repair_service_contracts;
 for(const auto&row:s.contracts.rows())if(same(row.second.terms.customer_acceptance_source,t.customer_acceptance_source)||same(row.second.terms.provider_acceptance_source,t.provider_acceptance_source)){
  if(!same_repair_service_contract_terms(row.second.terms,t))fail("ConflictingRepairServiceAcceptanceReplay",row.first);const auto&r=contract(x,{repair_service_contract_source(row.first),1});return {true,repair_service_contract_source(r.id),{}};
 }
 take(validate_repair_service_contract_proposal(x,t));
 if(t.custody_terms.uncollected_fee_qian)fail("RepairActualCustodyFeeOccurrenceWriterRequired",t.instance_id,ErrorKind::unsupported_capability);
 RepairServiceContractSource row{take(c.next_id("repair_service_contract")),now(x),t};const auto result=repair_service_contract_source(row.id);append(c,result);if(!s.contracts.insert(result.id,std::move(row)))fail("DuplicateRepairServiceContract",result.id);contract(x,{result,1});return {true,result,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
