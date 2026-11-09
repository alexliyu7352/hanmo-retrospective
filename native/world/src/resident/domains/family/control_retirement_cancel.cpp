#include "control_retirement_cancel.hpp"
#include "voluntary_succession.hpp"
#include "control_retirement_cancel_codec_fields.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <limits>
namespace hanmo::resident {
Outcome<PreparedPendingRetirementCancellation> read_cancel_pending_retirement(const CommonStore& common,const FamilyState& family,const ControlContinuationState& controls,const ControlContinuationCatalog& catalog,const CatalogIdentity& identity,const NativeContinuationOriginalSourceRead& sources,const StableId& actor,const CancelPendingRetirement& request){
 if(!sources.bound_to(common,family,controls))return {false,{},common_failure("ContinuationCandidateMismatch",{},ErrorKind::internal).error};
 auto valid=validate_control_continuation(common,controls,catalog,identity,sources);if(!valid.ok)return {false,{},valid.error};
 const auto* pending=controls.pending_continuations.find(request.pending_id);if(!pending)return {false,{},common_failure("NoContinuationPending").error};
 if(pending->status!=ContinuationStatus::open)return {false,{},common_failure("ContinuationAlreadyResolved").error};
 if(pending->mode!=ContinuationMode::voluntary_retirement)return {false,{},common_failure("ContinuationRetirementRequired").error};
 if(actor!=common.state().metadata.player_actor_id||actor!=controls.control_binding.actor_id||actor!=pending->predecessor_actor_id)return {false,{},common_failure("ContinuationPersonalCancellationRequired").error};
 if(request.expected_control_epoch!=controls.control_binding.control_epoch||request.expected_control_epoch!=pending->control_epoch_at_open)return {false,{},common_failure("StaleControlEpoch").error};
 if(request.expected_pending_generation!=pending->generation)return {false,{},common_failure("StaleContinuationGeneration").error};
 if(pending->generation==std::numeric_limits<Integer>::max())return {false,{},common_failure("ContinuationGenerationOverflow",{},ErrorKind::internal).error};
 auto death=sources.read_actor_death(actor);if(!death.ok)return {false,{},death.error};const auto* person=common.actor(actor);if(!person||!person->alive||death.value)return {false,{},common_failure("ContinuationLivingPredecessorRequired").error};
 auto now=sources.actual_now();if(!now.ok)return {false,{},now.error};if(now.value.half_day_tick!=common.tick())return {false,{},common_failure("ControlStableBoundaryRequired",{},ErrorKind::internal).error};
 const auto& original=pending->source_ref;if(original.domain!="family"||original.kind!="family_succession_consent"||original.ordinal)return {false,{},common_failure("ContinuationOriginalRetirementPreparationRequired",{},ErrorKind::unsupported_capability).error};
 const auto* value=family.facts.find(original.fact_id);const auto* plan=value?std::get_if<SuccessionConsentFact>(value):nullptr;if(!plan||plan->conclusion)return {false,{},common_failure("ContinuationPreparationResolved").error};
 return {true,{pending,plan},{}};
}
ProducedPendingRetirementCancelled::ProducedPendingRetirementCancelled(const PendingRetirementCancelledBuildView& view)
 :copied_fields_([&]{return CheckpointMemoryCharge(0);}()),
  receipt_{checkpoint_fresh_string_copy(view.pending_id),view.generation,
   {checkpoint_fresh_string_copy(view.original_withdrawn_preparation.domain),
    checkpoint_fresh_string_copy(view.original_withdrawn_preparation.kind),
    checkpoint_fresh_string_copy(view.original_withdrawn_preparation.fact_id),
    view.original_withdrawn_preparation.ordinal}}{}
Outcome<ProducedPendingRetirementCancelled> cancel_pending_retirement_in_candidate(CommonStore& common,FamilyState& family,ControlContinuationState& controls,const ControlContinuationCatalog& catalog,const CatalogIdentity& identity,const NativeContinuationOriginalSourceRead& sources,const StableId& actor,const CancelPendingRetirement& request){
 auto prepared=read_cancel_pending_retirement(common,family,controls,catalog,identity,sources,actor,request);if(!prepared.ok)return {false,{},prepared.error};
 const auto& original=prepared.value.pending->source_ref;
 ProducedPendingRetirementCancelled produced{PendingRetirementCancelledBuildView{
  prepared.value.pending->pending_id,prepared.value.pending->generation+1,original}};
 auto* pending=controls.pending_continuations.find(request.pending_id);
 // Preserve the original cancellation writer and source. No resources, actor
 // identity, current route, Office or consent from the successor is modified.
 auto valid=cancel_succession_plan(common,family,actor,original.fact_id);if(!valid.ok)return {false,{},valid.error};
 pending->boundary_work.reset();pending->status=ContinuationStatus::cancelled;++pending->generation;
 // Full Control3 cold/source validation runs only after the original outer
 // seal has inserted this typed result. There is no transient fake receipt.
 valid=sources.validate_voluntary_preparation(*pending);if(!valid.ok)return {false,{},valid.error};
 return {true,std::move(produced),{}};
}
Status validate_pending_retirement_cancelled_shape(const PendingRetirementCancelled& value){const auto& source=value.original_withdrawn_preparation;return continuation_codec::ref(value.pending_id)&&value.generation>=2&&source.domain=="family"&&source.kind=="family_succession_consent"&&continuation_codec::ref(source.fact_id)&&!source.ordinal?common_success():common_failure("InvalidPendingRetirementCancelled",value.pending_id,ErrorKind::internal);}
Outcome<CancelPendingRetirement> decode_cancel_pending_retirement(const BoundaryValue& value){CancelPendingRetirement out;ActorFoundationJsonVisitor c(value);auto valid=retirement_cancel_codec::request(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Status visit_pending_retirement_cancelled(CodecVisitor& c,PendingRetirementCancelled& value){return retirement_cancel_codec::receipt(c,value);}
Status emit_pending_retirement_cancelled_fields(ProjectionWriter& w,const PendingRetirementCancelled& value){ActorFoundationProjectionArchive c(w);auto valid=retirement_cancel_codec::receipt(c,const_cast<PendingRetirementCancelled&>(value));return valid.ok?c.finish():valid;}
Outcome<BoundaryValue> encode_pending_retirement_cancelled(const PendingRetirementCancelled& value){BoundaryValue out;BoundaryProjectionWriter w(out);auto valid=w.object("",[&](ProjectionWriter& root){return emit_pending_retirement_cancelled_fields(root,value);});if(valid.ok)valid=w.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<PendingRetirementCancelled> decode_pending_retirement_cancelled(const BoundaryValue& value){PendingRetirementCancelled out;ActorFoundationJsonVisitor c(value);auto valid=retirement_cancel_codec::receipt(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<CommonCommandSourceView> read_original_pending_retirement_withdrawal(const CommonStore& common,const FamilyState& family,const ControlContinuationState& controls,const StableId& pending_id){
 auto fail=[&](const StableId& code,ErrorKind kind=ErrorKind::internal)->Outcome<CommonCommandSourceView>{return {false,{},common_failure(code,pending_id,kind).error};};
 const auto* pending=controls.pending_continuations.find(pending_id);if(!pending||pending->mode!=ContinuationMode::voluntary_retirement||pending->status!=ContinuationStatus::cancelled)return fail("ContinuationCancelledRetirementRequired");
 const auto* fact=family.facts.find(pending->source_ref.fact_id);const auto* plan=fact?std::get_if<SuccessionConsentFact>(fact):nullptr;
 if(!plan||!plan->conclusion||plan->conclusion->kind!="cancelled"||plan->agreement.personal.predecessor_actor_id!=pending->predecessor_actor_id)return fail("ContinuationCancellationSourceRequired");
 CommonCommandSourceView found;for(const auto& row:common.state().results.rows())if(row.second&&row.second->details)if(const auto* payload=std::get_if<PendingRetirementCancelled>(&row.second->details->data);payload&&payload->pending_id==pending_id){
  auto shape=validate_pending_retirement_cancelled_shape(*payload);if(!shape.ok)return {false,{},shape.error};
  auto actual=read_common_command_source(common,{"common","command_result",row.second->result_id});if(!actual.ok)return actual;
  if(found.result||actual.value.result->kind!="cancel_pending_retirement"||actual.value.result->tick!=plan->conclusion->tick||actual.value.history->actor_id!=pending->predecessor_actor_id||payload->generation!=pending->generation||!same_continuation_cause(payload->original_withdrawn_preparation,pending->source_ref))return fail("ContinuationWithdrawalCommandMismatch");found=actual.value;
 }
 if(!found.result)return fail("ContinuationOriginalWithdrawalReceiptRequired",ErrorKind::unsupported_capability);
 return {true,found,{}};
}

}
