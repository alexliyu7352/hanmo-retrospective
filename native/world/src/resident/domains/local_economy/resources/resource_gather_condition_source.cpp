#include "resource_gather_condition_source.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
SourceRef resource_gather_claim_source(const StableId&id){return {"local_economy","resource_gather_claim",id};}
SourceRef resource_gather_accepted_source(const StableId&id){return {"local_economy","resource_gather_accepted",id};}
SourceRef resource_gather_receipt_source(const StableId&id){return {"local_economy","resource_gather_receipt",id};}
namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
[[noreturn]]void fail(const char* code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
std::size_t position(const CommonStore&c,const SourceRef&ref){
 const auto*header=c.state().facts.find(ref.id);const auto&order=c.state().facts.order();
 const auto it=std::find(order.begin(),order.end(),ref.id);
 if(!header||header->id!=ref.id||header->domain!=ref.domain||header->kind!=ref.kind||it==order.end()||std::count(order.begin(),order.end(),ref.id)!=1)fail("GatherOriginalHeaderRequired",ref.id);
 return static_cast<std::size_t>(it-order.begin());
}
}
Outcome<ResourceGatherConditionSource> read_gather_period_for_condition(
 const ResourceGatherConditionContext&x,const SourceRef&source){try{
 if(!same_source_ref(source,resource_gather_receipt_source(source.id)))fail("GatherConditionReceiptKindMismatch",source.id);
 take(validate_resource_gather_owner_values(x.gather));
 const auto now=take(x.clock.read_current_moment());take(encode_world_moment(now));
 if(now.half_day_tick<x.common.tick())fail("GatherConditionClockMismatch",source.id);
 // Structural fixed-history validation authenticates earned credit and original
 // transitions, but does not callback any Condition result or stock writer.
 take(validate_fixed_history_values(x.elapsed,now,x.scale));
 const ResourceGatherReceipt*receipt=nullptr;
 for(const auto&r:x.gather.receipts)if(r.receipt_id==source.id){if(receipt)fail("DuplicateGatherConditionReceipt",source.id);receipt=&r;}
 if(!receipt)fail("GatherConditionOriginalReceiptRequired",source.id);
 const ResourceGatherClaim*claim=nullptr;for(const auto&v:x.gather.claims)if(v.claim_id==receipt->claim_id){if(claim)fail("DuplicateGatherConditionClaim",source.id);claim=&v;}
 if(!claim)fail("GatherConditionOriginalClaimRequired",source.id);
 const ResourceGatherAcceptedTerms*accepted=nullptr;for(const auto&v:x.gather.accepted_terms)if(v.acceptance_id==claim->accepted_terms_ref){if(accepted)fail("DuplicateGatherConditionAcceptance",source.id);accepted=&v;}
 if(!accepted||accepted->claim_id!=claim->claim_id||receipt->method!=accepted->method)fail("GatherConditionAcceptanceMismatch",source.id);
 const auto accepted_pos=position(x.common,resource_gather_accepted_source(accepted->acceptance_id));
 const auto claim_pos=position(x.common,resource_gather_claim_source(claim->claim_id));
 const auto receipt_pos=position(x.common,source);
 if(accepted_pos>=claim_pos||claim_pos>=receipt_pos)fail("GatherConditionOriginalOrderMismatch",source.id);
 if(take(compare_world_moments(receipt->applied_at,now))>0||take(compare_world_moments(accepted->accepted_at,receipt->actual_period.from))>0||take(compare_world_moments(receipt->applied_at,receipt->actual_period.until))<0)fail("GatherConditionActualMomentMismatch",source.id);
 const ActorActionWorkBinding binding{accepted->action_id,"gather_resource",resource_gather_claim_source(claim->claim_id)};
 if(!same_work_binding(receipt->actual_period.binding,WorkExecutionBinding{binding}))fail("GatherConditionActionSourceMismatch",source.id);
 if(const auto*action=x.common.action(accepted->action_id))if(action->actor_id!=accepted->actor_id||action->kind!="gather_resource")fail("GatherConditionActualActorMismatch",source.id);
 const auto* elapsed=find_fixed_elapsed(x.elapsed,accepted->action_id);
 if(!elapsed||elapsed->execution_source_ref!=claim->claim_id||
    take(compare_world_moments(elapsed->started_at,accepted->accepted_at))!=0)
  fail("GatherConditionElapsedOriginalMismatch",source.id);
 if(!x.common.actor(accepted->actor_id))fail("GatherConditionOriginalActorMissing",source.id);
 auto period=take(read_fixed_service_work_period2(x.elapsed,binding,receipt->actual_period.period_ordinal,accepted->accepted_half_days,now,x.scale));
 if(!take(same_work_period_ref2(receipt->actual_period,period.ref)))fail("GatherConditionFixedPeriodMismatch",source.id);
 const auto method=take(read_gather_method_policy_v1(accepted->method));
 return {true,{accepted,receipt,std::move(period),method.fatigue_delta_per_completed_half_day},{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}
 catch(const std::bad_alloc&){return {false,{},common_failure("GatherConditionReadExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("GatherConditionReadExhausted").error};}}
}
