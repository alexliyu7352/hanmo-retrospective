#include "craft_condition_effects.hpp"
#include "craft_character_sources.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
const CharacterConditionSources& body(const CraftJobReadContext&x){
 if(!x.character_sources)fail("CraftCharacterSourceProviderRequired",{},ErrorKind::unsupported_capability);
 const auto&sources=x.character_sources->body;
 if(&sources.common!=&x.materials.common||sources.craft!=&x||&x.character_sources->jobs!=&x.materials.jobs||take(compare_world_moments(sources.actual_now,take(x.clock.read_current_moment()))))fail("CraftConditionReadContextMismatch");
 return sources;
}
}
Status validate_craft_gate_body_effects(const CraftJobReadContext&x,const CraftGateApplied&gate){try{
 const auto&sources=body(x);const auto*actual=take(read_craft_gate_for_original_effect(x,gate.id));const auto*job=x.materials.jobs.jobs.find(gate.job_id);
 if(actual!=&gate||!job||gate.result.exertion_source_refs.size()!=job->terms.accepted_helpers.size()+1)fail("CraftActualExertionCoverageMismatch",gate.id);
 std::size_t index=0;auto participant=[&](const StableId&actor){const auto&id=gate.result.exertion_source_refs[index++];const auto*receipt=sources.state.character_condition_extensions.applied_receipts.find(id);if(!receipt||receipt->result_id!=id||receipt->actor_id!=actor||!receipt->capability_application)fail("CraftActualConditionReceiptRequired",id);take(validate_craft_gate_condition_receipt(sources,*receipt));const auto*cause=std::get_if<WorkOrStudyConditionCause>(&receipt->cause);if(!cause||cause->kind!=WorkOrStudyConditionCause::Kind::craft_gate||cause->action_id!=job->execution_binding.action_id||cause->ordinal<1||cause->ordinal-1!=gate.result.gate_ordinal||take(compare_world_moments(receipt->effective_at.exact_moment,gate.occurred_at)))fail("CraftActualExertionGateMismatch",id);};
 participant(job->terms.maker.id);for(const auto&helper:job->terms.accepted_helpers)participant(helper);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status apply_craft_gate_conditions_in_candidate(CommonStore&common,CraftJobsState&jobs,const CraftJobReadContext&read,const CraftGateConditionWriteContext&write,const StableId&id){try{
 const auto&sources=body(read);if(&read.materials.common!=&common||&read.materials.jobs!=&jobs||&sources!=&write.body||&write.foundation!=&sources.state||!sources.permanent_growth||!write.capability.bound_to(common,*sources.permanent_growth))fail("CraftConditionWriteCandidateMismatch",id);
 const auto*input=take(read_craft_gate_for_original_effect(read,id));auto*gate=jobs.gates.find(id);const auto*job=jobs.jobs.find(input->job_id);
 if(!gate||input!=gate||!job||take(compare_world_moments(gate->occurred_at,take(write.stamps.read_current_moment())))||take(compare_world_moments(gate->occurred_at,sources.actual_now))||gate->result.exertion_source_refs.size()>job->terms.accepted_helpers.size()+1)fail("CraftConditionActualGateBoundaryRequired",id);
 std::size_t index=0;auto participant=[&](const StableId&actor){
  // Prior actors really changed the ConditionHead/capability revision. Every
  // next actor receives a fresh candidate stamp; no group-wide stale token.
  const auto stamp=take(write.stamps.capture({{ResourceKey::Kind::actor,actor}},{{"actor_foundation","condition_head",actor}},{id}));
  auto applied=take(apply_verified_craft_gate_condition(common,write.foundation,sources,read,craft_gate_source(id),actor,*stamp,write.stamp_authority,write.versions,write.capability));
  if(applied.already_applied){if(!applied.actual_writes.empty())fail("CraftConditionReplayHasNewWrites",id);}else{if(applied.actual_writes.size()!=1||applied.actual_writes.front().receipt_ref.result_id!=applied.receipt_ref.result_id)fail("CraftConditionActualWriteBindingMismatch",id);write.condition_writes.push_back(std::move(applied.actual_writes.front()));}
  if(index<gate->result.exertion_source_refs.size()){if(gate->result.exertion_source_refs[index]!=applied.receipt_ref.result_id)fail("ConflictingCraftConditionReplay",id);}else gate->result.exertion_source_refs.push_back(std::move(applied.receipt_ref.result_id));
  ++index;
 };
 participant(job->terms.maker.id);for(const auto&helper:job->terms.accepted_helpers)participant(helper);
 return validate_craft_gate_body_effects(read,*gate);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
