#include "resident/domains/growth/growth_model.hpp"
#include "study_pending_release.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
struct StudyPendingReleaseProof::State {
 CheckpointMemoryCharge text_memory;
 const CommonStore* common=nullptr;
 const GrowthState* growth=nullptr;
 const AuthoredActorFoundationState* foundation=nullptr;
 std::shared_ptr<const ActorConditionCaptureIdentity> capture;
 ActorConditionSavepointBoundary savepoint;
 StableId action_id,receipt_id,death_id;
 Integer sequence=0;
 WorldMoment at;
};
namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
const ConditionReceiptV2& actual_capture(const CharacterConditionSources& body,const BoundActorConditionWrite& write,const StudyVerifiedParticipantDeath& cause,const WorldMoment& at){
 const auto capture=body.common.actor_condition_capture_identity();const auto& drafts=body.common.actor_condition_execution_drafts();
 if(!capture||capture!=write.capture_identity||write.capture_sequence<1||write.capture_sequence>static_cast<Integer>(drafts.size()))fail("StudyPendingReleaseCaptureRequired",cause.actor_id);
 const auto& draft=drafts[static_cast<std::size_t>(write.capture_sequence-1)];const auto* receipt=body.state.character_condition_extensions.applied_receipts.find(write.receipt_ref.result_id);
 if(!receipt||receipt->actor_id!=cause.actor_id||!receipt->life_transition_ref||*receipt->life_transition_ref!=cause.death_ref.id||receipt->effective_at.kind!=ActorFoundationAt::Kind::exact_moment||take(compare_world_moments(receipt->effective_at.exact_moment,at))||draft.sequence!=write.capture_sequence||draft.kind!=ActorConditionExecutionDraft::Kind::mutation||!draft.before||draft.actor_id!=cause.actor_id||!draft.alive_written||!draft.before->alive||draft.after.alive||draft.after.available)fail("StudyPendingReleaseActualDeathRequired",cause.actor_id);
 bool alive=false;for(const auto& delta:receipt->scalar_changes)if(delta.field==ConditionScalarChange::Field::alive){if(alive||delta.before!=1||delta.after!=0)fail("StudyPendingReleaseDeathDeltaMismatch",receipt->result_id);alive=true;}if(!alive)fail("StudyPendingReleaseDeathDeltaRequired",receipt->result_id);return *receipt;
}
}
StudyPendingReleaseProof::StudyPendingReleaseProof(std::shared_ptr<const State> state):state_(std::move(state)){}
const StableId* StudyPendingReleaseProof::action_id()const noexcept{return state_?&state_->action_id:nullptr;}
Status StudyPendingReleaseProof::validate(const CharacterConditionSources& body,const GrowthState& growth,const StudyTerminal& ref)const {try{
 const auto* p=state_.get();if(!p||p->common!=&body.common||p->growth!=&growth||body.growth!=&growth||p->foundation!=&body.state||p->action_id!=ref.action_id||!p->capture||p->capture!=body.common.actor_condition_capture_identity()||take(compare_world_moments(p->at,body.actual_now))>0)fail("StudyPendingReleaseProofExpired",ref.action_id);
 take(body.common.validate_actor_condition_savepoint_boundary(p->savepoint));const auto* raw=growth.facts.find(ref.action_id);const auto* fact=raw?std::get_if<StudyFact>(raw):nullptr;if(!fact||!fact->study_service_terminal||fact->study_service_terminal->reservation_release_results.size()!=1||!fact->study_service_terminal->reservation_release_results.front().original_release_commit_ref.result_id.empty())fail("StudyPendingReleaseOriginalSlotMismatch",ref.action_id);
 const auto& terminal=*fact->study_service_terminal;const auto* cause=std::get_if<StudyVerifiedParticipantDeath>(&terminal.stop_cause);if(!cause||cause->death_ref.id!=p->death_id||take(compare_world_moments(terminal.terminated_at,p->at)))fail("StudyPendingReleaseOriginalCauseMismatch",ref.action_id);
 actual_capture(body,{{p->receipt_id},p->capture,p->sequence},*cause,p->at);return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<StudyPendingReleaseProof> stop_study_for_verified_death_in_capture(CommonStore& common,GrowthState& growth,const CharacterConditionSources& body,const GrowthAccess& access,const StableId& id,const StudyVerifiedParticipantDeath& cause,const BoundActorConditionWrite& write,const ReadStamp& stamp,const VerifiedDeathRead& deaths,StudyOriginalActionReleaseWrite& release){try{
 if(&body.common!=&common||body.growth!=&growth||!body.growth_catalog)fail("StudyPendingReleaseSameCandidateRequired",id);actual_capture(body,write,cause,body.actual_now);
 const auto* raw=growth.facts.find(id);const auto* prior=raw?std::get_if<StudyFact>(raw):nullptr;if(!prior||prior->phase!="started"||prior->study_service_terminal)fail("StudyPendingReleaseNewTerminationRequired",id);
 auto text_memory=CheckpointMemoryCharge(0);auto p=checkpoint_make_shared<StudyPendingReleaseProof::State>();p->text_memory=std::move(text_memory);p->common=&common;p->growth=&growth;p->foundation=&body.state;p->capture=write.capture_identity;p->savepoint=take(common.actor_condition_savepoint_boundary());p->action_id=id;p->receipt_id=write.receipt_ref.result_id;p->death_id=cause.death_ref.id;p->sequence=write.capture_sequence;p->at=body.actual_now;
 take(stop_study_for_verified_death_in_candidate(common,growth,*body.growth_catalog,body.catalog,access,id,cause,body.actual_now,stamp,deaths,release));StudyPendingReleaseProof proof{std::move(p)};take(proof.validate(body,growth,{id}));return {true,std::move(proof),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
