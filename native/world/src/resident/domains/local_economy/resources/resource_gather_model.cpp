#include "resource_gather_model.hpp"
#include "resident/storage/common_store.hpp"
#include <set>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status s){if(!s.ok)throw Failure{s.error};}
void add(Integer&to,Integer n){if(n<0||__builtin_add_overflow(to,n,&to))fail("ResourceGatherQuantityOverflow");}
bool same(const ActorFoundationRuleRef&a,const ActorFoundationRuleRef&b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
void rule(const ActorFoundationRuleRef&r){if(r.id.empty()||r.version<1||r.catalog_digest.empty())fail("ResourceGatherPolicyRequired",r.id);}
void sources(const std::vector<SourceRef>&v){for(std::size_t i=0;i<v.size();++i){if(!valid_source_ref(v[i]))fail("ResourceGatherOriginalSourceRequired");for(std::size_t j=0;j<i;++j)if(same_source_ref(v[i],v[j]))fail("DuplicateResourceGatherSource",v[i].id);}}
}
Status validate_resource_gather_owner_values(const ResourceGatherOwnerState&s){try{
 std::set<StableId>accept_ids,claim_ids,actions,receipt_ids,disposition_ids,source_ids;
 for(const auto&t:s.accepted_terms){
  if(t.acceptance_id.empty()||t.claim_id.empty()||t.site_id.empty()||t.action_id.empty()||t.actor_id.empty()||t.item_id.empty()||t.recipient_inventory_id.empty()||
     !valid_source_ref(t.harvest_entitlement_source.source_ref)||t.harvest_entitlement_source.source_schema<1||!valid_source_ref(t.return_or_stop_policy)||
     t.requested_units<0||t.reserved_source_units<0||t.accepted_half_days<=0)
   fail("InvalidResourceGatherAcceptance",t.acceptance_id);
  rule(t.policy_ref);take(read_gather_method_policy_v1(t.method));take(encode_world_moment(t.accepted_at));
  if(!accept_ids.insert(t.acceptance_id).second||!claim_ids.insert(t.claim_id).second||!actions.insert(t.action_id).second||!source_ids.insert(t.acceptance_id).second)
   fail("DuplicateResourceGatherAcceptance",t.acceptance_id);
 }
 if(s.claims.size()!=s.accepted_terms.size())fail("ResourceGatherClaimAcceptanceCoverage");
 std::set<StableId>actual_claims;
 for(const auto&claim:s.claims){
  if(!actual_claims.insert(claim.claim_id).second||claim.remaining_reserved_units<0)fail("InvalidResourceGatherClaim",claim.claim_id);
  const ResourceGatherAcceptedTerms*t=nullptr;for(const auto&v:s.accepted_terms)if(v.claim_id==claim.claim_id)t=&v;
  if(!t||t->acceptance_id!=claim.accepted_terms_ref)fail("ResourceGatherClaimAcceptanceMismatch",claim.claim_id);
  Integer harvested=0,depleted=0,released=0;std::set<Integer>ordinals;
  std::optional<WorldMoment>terminal_at;
  std::size_t terminal_count=0;
  for(const auto&d:s.dispositions)if(d.claim_id==claim.claim_id){
   if(d.disposition_id.empty()||!valid_source_ref(d.original_action_disposition)||d.released_source_units<0)
    fail("InvalidResourceGatherDisposition",d.disposition_id);
   take(encode_world_moment(d.occurred_at));if(take(compare_world_moments(d.occurred_at,t->accepted_at))<0)fail("ResourceGatherDispositionBeforeAcceptance",d.disposition_id);
   add(released,d.released_source_units);
   if(d.terminal){++terminal_count;terminal_at=d.occurred_at;
    if(!claim.terminal_disposition_ref||!valid_source_ref(*claim.terminal_disposition_ref)||claim.terminal_disposition_ref->id!=d.disposition_id)
     fail("ResourceGatherTerminalSourceMismatch",claim.claim_id);
   }
  }
  if(terminal_count>1||bool(claim.terminal_disposition_ref)!=(terminal_count==1)||
     (terminal_count&&claim.remaining_reserved_units))fail("ResourceGatherTerminalCoverage",claim.claim_id);
  for(const auto&r:s.receipts)if(r.claim_id==claim.claim_id){
   if(r.receipt_id.empty()||r.method!=t->method||!same(r.policy_ref,t->policy_ref)||r.harvested_units<0||r.site_depletion_units<0||r.loss_units<0||r.worker_input_sources.empty())
    fail("InvalidResourceGatherReceipt",r.receipt_id);
   take(validate_work_period_ref2(r.actual_period));take(encode_world_moment(r.applied_at));
   const auto*action=std::get_if<ActorActionWorkBinding>(&r.actual_period.binding);
   if(!action||action->action_id!=t->action_id||action->source_owner_ref.id!=claim.claim_id||
      r.actual_period.period_ordinal>=t->accepted_half_days||!ordinals.insert(r.actual_period.period_ordinal).second||
      take(compare_world_moments(r.actual_period.from,t->accepted_at))<0||take(compare_world_moments(r.applied_at,r.actual_period.until))<0||
      (terminal_at&&take(compare_world_moments(r.applied_at,*terminal_at))>0))fail("ResourceGatherActualPeriodMismatch",r.receipt_id);
   Integer total=r.harvested_units;add(total,r.loss_units);if(total!=r.site_depletion_units)fail("ResourceGatherDepletionMismatch",r.receipt_id);
   add(harvested,r.harvested_units);add(depleted,r.site_depletion_units);
   sources(r.output_lot_refs);sources(r.lot_properties_refs);sources(r.tool_wear_refs);sources(r.condition_application_refs);sources(r.resource_effect_refs);
   if(!valid_source_ref(r.observation_result_ref)||
      (r.harvested_units==0&&(!r.output_lot_refs.empty()||!r.lot_properties_refs.empty()||!r.zero_output_reason||!valid_source_ref(*r.zero_output_reason)))||
      (r.harvested_units>0&&(r.output_lot_refs.empty()||r.lot_properties_refs.empty()||r.zero_output_reason)))fail("ResourceGatherOutputCoverage",r.receipt_id);
   for(std::size_t i=0;i<r.worker_input_sources.size();++i){const auto&w=r.worker_input_sources[i];
    if(!valid_source_ref(w.source_ref)||w.source_schema<1)fail("ResourceGatherWorkerSourceRequired",r.receipt_id);
    for(std::size_t j=0;j<i;++j)if(same_source_ref(w.source_ref,r.worker_input_sources[j].source_ref))fail("DuplicateResourceGatherWorkerSource",r.receipt_id);
   }
  }
  Integer expected_ordinal=0;for(auto ordinal:ordinals){if(ordinal!=expected_ordinal)fail("ResourceGatherPeriodCoverage",claim.claim_id);if(expected_ordinal==std::numeric_limits<Integer>::max())fail("ResourceGatherOrdinalOverflow",claim.claim_id);++expected_ordinal;}
  Integer accounted=claim.remaining_reserved_units;add(accounted,depleted);add(accounted,released);
  if(accounted!=t->reserved_source_units||harvested>t->requested_units)fail("ResourceGatherClaimConservation",claim.claim_id);
 }
 for(const auto&r:s.receipts)if(!actual_claims.contains(r.claim_id)||!receipt_ids.insert(r.receipt_id).second||!source_ids.insert(r.receipt_id).second)fail("OrphanOrDuplicateResourceGatherReceipt",r.receipt_id);
 for(const auto&r:s.dispositions)if(!actual_claims.contains(r.claim_id)||!disposition_ids.insert(r.disposition_id).second||!source_ids.insert(r.disposition_id).second)fail("OrphanOrDuplicateResourceGatherDisposition",r.disposition_id);
 return common_success();
 }catch(const Failure&f){return {false,{},f.error};}}
}
