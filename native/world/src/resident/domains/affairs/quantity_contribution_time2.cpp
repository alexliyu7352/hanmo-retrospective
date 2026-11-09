#include "quantity_contribution_time2.hpp"
#include <limits>
namespace hanmo::resident {
namespace {bool same(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}}
Status apply_quantity_contribution_v2(const CommonStore& common,AffairsState& state,const TaskState& tasks,const StableId& id){
 if(!state.exact_claims||!tasks.exact_quantities)return common_failure("ExactQuantityCapabilityRequired",id,ErrorKind::unsupported_capability);
 const auto* d=tasks.deliveries.find(id);const auto* task=d?tasks.tasks.find(d->task_id):nullptr;const auto* c=task&&task->contract_id?tasks.contracts.find(*task->contract_id):nullptr;const auto* t=d?tasks.transfers.find(d->transfer_fact_id):nullptr;
 if(!d||!task||task->record_kind!=TaskRecordKind::exact_quantity||!c||!t||d->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||t->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||!d->completed_at||!t->completed_at)return common_failure("ExactProcurementSourceMissing",id,ErrorKind::internal);
 if(c->frozen_terms.definition.template_id!="procurement"||t->purpose!="delivery")return common_failure("NonQuantityDeliveryPurpose",id,ErrorKind::unsupported_capability);
 const auto* dh=common.state().facts.find(d->id);const auto* th=common.state().facts.find(t->id);if(!dh||!th||dh->domain!="tasks"||dh->kind!="task_delivery"||th->domain!="tasks"||th->kind!="task_transfer")return common_failure("ExactProcurementHeaderMismatch",id,ErrorKind::internal);
 const auto& b=c->source_binding;auto* n=state.needs.find(b.need_id);auto* claim=state.claims.find(b.claim_id);const auto& terms=c->frozen_terms;
 if(!n||!claim||!claim->accepted_source||!std::holds_alternative<TaskClaimEstablishedRef>(*claim->accepted_source)||claim->contract_id!=c->id||claim->need_id!=n->id||n->affair_id!=b.affair_id||b.contract_id!=c->id||n->measure==NeedMeasure::outstanding_obligation)return common_failure("DeliverySourceMismatch",id);
 auto accepted=read_need_claim_acceptance_at(common,tasks,state,claim->id);if(!accepted.ok)return {false,{},accepted.error};auto order=compare_world_moments(accepted.value,*d->completed_at);if(!order.ok)return {false,{},order.error};auto equal=compare_world_moments(*d->completed_at,*t->completed_at);if(!equal.ok)return {false,{},equal.error};
 if(order.value>0||equal.value||d->completed_at->half_day_tick!=d->completed_tick||t->completed_at->half_day_tick!=t->completed_tick||d->activity_id!=c->org_activity_id.value_or(StableId{})||d->quantity<=0||d->to_inventory_id!=terms.target_inventory_id||d->item_id!=terms.item_id||!same(d->beneficiary,terms.beneficiary)||!same(n->beneficiary,terms.beneficiary)||t->contract_id!=c->id||t->task_id!=d->task_id||t->source_action_id!=d->source_action_id||t->from_inventory_id!=d->from_inventory_id||t->to_inventory_id!=d->to_inventory_id||t->item_id!=d->item_id||t->quantity!=d->quantity||!owner_equal(t->to_owner,c->issuer)||t->from_owner.kind!=Owner::Kind::task||t->from_owner.id!=task->id)return common_failure("DeliveryAcceptanceMismatch",id);
 // A delivery cannot be accepted before S3's actual acceptance terminal or after an unestablished task.
 if(task->status=="accepting"||!task->accepted_at)return common_failure("TaskAcceptanceNotCompleted",id);auto ready=compare_world_moments(*task->accepted_at,*d->completed_at);if(!ready.ok)return {false,{},ready.error};if(ready.value>0)return common_failure("DeliveryBeforeTaskAcceptance",id);
 if(const auto* used=state.contributions.find(id))return used->transfer_fact_id==t->id&&used->contract_id==c->id&&used->claim_id==claim->id&&used->need_id==n->id&&used->counted_amount==d->quantity?common_success():common_failure("DeliveryAlreadyContributed",id);
 for(const auto& row:state.contributions.rows())if(row.second.transfer_fact_id==t->id)return common_failure("TransferAlreadyContributed",t->id);
 if(claim->fulfilled_amount<0||claim->fulfilled_amount>claim->accepted_amount||d->quantity>claim->accepted_amount-claim->fulfilled_amount||n->fulfilled_amount>std::numeric_limits<Integer>::max()-d->quantity||n->revision==std::numeric_limits<Integer>::max())return common_failure("NeedQuantityOverflow",id);
 claim->fulfilled_amount+=d->quantity;if(claim->fulfilled_amount==claim->accepted_amount)claim->status=ClaimStatus::settled;else if(claim->status==ClaimStatus::accepted||claim->status==ClaimStatus::in_progress)claim->status=ClaimStatus::in_progress;
 n->fulfilled_amount+=d->quantity;++n->revision;n->receipt_ids.push_back(id);state.contributions.insert(id,{id,t->id,c->id,claim->id,n->id,d->quantity});return common_success();
}
Status reconcile_quantity_contributions_v2(const CommonStore& common,AffairsState& affairs,const TaskState& tasks){
 for(const auto& row:tasks.deliveries.rows()){
  const auto* task=tasks.tasks.find(row.second.task_id);const auto* contract=task&&task->contract_id?tasks.contracts.find(*task->contract_id):nullptr;
  if(!contract)return common_failure("UnboundProcurementDelivery",row.first,ErrorKind::internal);
  if(contract->frozen_terms.definition.template_id!="procurement")return common_failure("QuantityDeliveryDispatchMismatch",row.first,ErrorKind::unsupported_capability);
  auto r=task->record_kind==TaskRecordKind::exact_quantity?apply_quantity_contribution_v2(common,affairs,tasks,row.first):task->record_kind==TaskRecordKind::legacy_quantity?apply_contribution(affairs,tasks,row.second):common_failure("NonQuantityTaskContribution",row.first,ErrorKind::internal);if(!r.ok)return r;
 }return common_success();
}
}
