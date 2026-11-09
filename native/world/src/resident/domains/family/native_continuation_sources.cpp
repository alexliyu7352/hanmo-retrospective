#include "native_continuation_sources.hpp"
#include "apprenticeship_legacy_basis.hpp"
#include "voluntary_succession.hpp"
#include "control_retirement_cancel.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
Status bad(const StableId& code,const StableId& id={}){return common_failure(code,id,ErrorKind::internal);}
bool precedes(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
Outcome<CauseRef> actual_fact(const CommonStore& c,const StableId& id){const auto* h=c.state().facts.find(id);if(!h||h->id!=id)return {false,{},bad("ContinuationOriginalFactRequired",id).error};return {true,{h->domain,h->kind,id,{}},{}};}
ConsentRef consent_ref(const StableId& id,const LegacyControlDecisionSource& s){return {s.request_id,s.terms_version,s.successor_actor_id,id};}
bool same_biological(const BiologicalControlOrigin& a,const BiologicalChildOrigin& b){return a.kind==b.kind&&a.parent_actor_id==b.parent_actor_id&&a.child_actor_id==b.child_actor_id&&a.original_catalog_actor_id==b.original_catalog_actor_id&&a.birth_fact_id==b.birth_fact_id&&a.creation_fact_id==b.creation_fact_id&&a.source_available_tick==b.source_available_tick;}
Outcome<std::pair<ContinuationPath,CauseRef>> path(const NativeContinuationSources& x,const LegacyControlDecisionSource& s){
 if(s.biological_origin){const auto& b=*s.biological_origin;if(b.kind==BiologicalChildOriginKind::installed_lineage)return {true,{ContinuationPath::biological,{"family_lineage","installed_origin",b.original_catalog_actor_id,{}}},{}};auto r=actual_fact(x.common,b.birth_fact_id);if(!r.ok)return {false,{},r.error};return {true,{ContinuationPath::biological,std::move(r.value)},{}};}
 auto basis=actual_fact(x.common,s.relation_fact_id);if(!basis.ok)return {false,{},basis.error};
 if(s.collateral_dialogue)return {true,{ContinuationPath::collateral,std::move(basis.value)},{}};
 if(x.disciple&&x.disciple->catalog.enabled&&x.disciple->catalog.requests.find(s.request_id))return {true,{ContinuationPath::disciple,std::move(basis.value)},{}};
 if(basis.value.domain!="family")return {false,{},bad("ContinuationAdoptionSourceRequired").error};if(basis.value.kind=="family_adult_adoption")return {true,{ContinuationPath::adult_adopted,std::move(basis.value)},{}};if(basis.value.kind=="family_adoption")return {true,{ContinuationPath::adopted,std::move(basis.value)},{}};return {false,{},bad("ContinuationBasisInvalid").error};
}
}
bool NativeContinuationFoundationSourceRead::bound_to(const CommonStore& c)const noexcept{return &source_.common==&c&&source_.people.bound_to(c,source_.deaths)&&source_.people.control_continuation()==&source_.controls;}
bool NativeContinuationFoundationSourceRead::bound_to(const CommonStore& c,const FamilyState& family,const ControlContinuationState& controls)const noexcept{return bound_to(c)&&&source_.family==&family&&&source_.controls==&controls;}
Outcome<WorldMoment> NativeContinuationFoundationSourceRead::actual_now()const{return source_.people.read_current_moment();}
Status NativeContinuationFoundationSourceRead::validate_initial_control_actor(const StableId& actor)const{if(actor!=source_.family.founder_actor_id)return bad("ContinuationInitialActorMismatch",actor);return validate_family_founder_v2(source_.common,source_.family,source_.family_catalog,source_.common_catalog,source_.origins);}
Outcome<VerifiedDeathView> NativeContinuationFoundationSourceRead::read_death(const DeathRef& ref)const{
 if(ref.domain!="family_life"||ref.kind!="family_death")return {false,{},bad("ContinuationDeathRefRequired").error};const auto* row=source_.deaths.deaths.find(ref.id);if(!row)return {false,{},bad("ContinuationDeathRefRequired",ref.id).error};auto death=source_.people.life().read_actor_death(row->actor_id);if(!death.ok)return {false,{},death.error};if(!death.value||death.value->death_ref.id!=ref.id)return {false,{},bad("ContinuationDeathRefMismatch",ref.id).error};return {true,std::move(*death.value),{}};
}
Outcome<std::optional<VerifiedDeathView>> NativeContinuationFoundationSourceRead::read_actor_death(const StableId& actor)const{return source_.people.life().read_actor_death(actor);}
Status NativeContinuationFoundationSourceRead::validate_pending_control_origin(const PendingContinuation& p)const{
 const auto* h=source_.common.state().facts.find(p.source_ref.fact_id);if(!h||h->domain!=p.source_ref.domain||h->kind!=p.source_ref.kind)return bad("ContinuationPendingOriginalSourceRequired",p.pending_id);
 auto actor=read_control_actor_before_original_fact(source_.common,source_.controls,source_.family.founder_actor_id,{h->domain,h->kind,h->id});if(!actor.ok)return {false,{},actor.error};if(actor.value!=p.predecessor_actor_id)return bad("ContinuationPendingControllerMismatch",p.pending_id);
 Integer epoch=0;for(const auto& a:source_.controls.activation_records.rows())if(precedes(source_.common,a.first,h->id))epoch=a.second.new_control_epoch;return epoch==p.control_epoch_at_open?common_success():bad("ContinuationPendingEpochMismatch",p.pending_id);
}
// 完整接续入口只委托地基正文，避免两套控制/死亡来源规则分叉。
bool NativeContinuationOriginalSourceRead::bound_to(const CommonStore& common)const noexcept{
 return foundation_.bound_to(common);
}
bool NativeContinuationOriginalSourceRead::bound_to(
    const CommonStore& common,const FamilyState& family,
    const ControlContinuationState& controls)const noexcept{
 return foundation_.bound_to(common,family,controls);
}
Outcome<WorldMoment> NativeContinuationOriginalSourceRead::actual_now()const{
 return foundation_.actual_now();
}
Status NativeContinuationOriginalSourceRead::validate_initial_control_actor(
    const StableId& actor)const{
 return foundation_.validate_initial_control_actor(actor);
}
Outcome<VerifiedDeathView> NativeContinuationOriginalSourceRead::read_death(
    const DeathRef& ref)const{
 return foundation_.read_death(ref);
}
Outcome<std::optional<VerifiedDeathView>>
NativeContinuationOriginalSourceRead::read_actor_death(const StableId& actor)const{
 return foundation_.read_actor_death(actor);
}
Status NativeContinuationOriginalSourceRead::validate_pending_control_origin(
    const PendingContinuation& pending)const{
 return foundation_.validate_pending_control_origin(pending);
}
Status NativeContinuationOriginalSourceRead::validate_voluntary_preparation(const PendingContinuation& p)const{
 const auto& x=source_;if(p.mode!=ContinuationMode::voluntary_retirement||p.source_ref.domain!="family"||p.source_ref.kind!="family_succession_consent"||p.source_ref.ordinal)return common_failure("ContinuationOriginalRetirementPreparationRequired",{},ErrorKind::unsupported_capability);
 const auto* f=x.family.facts.find(p.source_ref.fact_id);const auto* plan=f?std::get_if<SuccessionConsentFact>(f):nullptr;const auto* h=x.common.state().facts.find(p.source_ref.fact_id);
 if(!plan||!h||h->domain!=p.source_ref.domain||h->kind!=p.source_ref.kind||plan->agreement.personal.predecessor_actor_id!=p.predecessor_actor_id||plan->agreement.accepted_tick>p.created_tick)return bad("InvalidContinuationPreparation",p.source_ref.fact_id);
 if(!x.family_catalog.voluntary_succession)return common_failure("ContinuationVoluntarySourceRequired",{},ErrorKind::unsupported_capability);
 // Nonzero asset terms are a different original transaction. Control2 cannot
 // silently drop an accepted gift nor execute it as a side effect of control.
 if(plan->agreement.personal.money!=0||!plan->agreement.personal.lots.empty())return common_failure("ContinuationSeparateAssetAgreementRequired");
 auto valid=validate_succession_consent_source(x.common,x.family,x.common_catalog,x.interactions,*x.family_catalog.voluntary_succession,plan->agreement);if(!valid.ok)return valid;
 if(plan->conclusion){if(p.status!=ContinuationStatus::cancelled||plan->conclusion->kind!="cancelled"||p.generation<2)return common_failure("ContinuationPreparationResolved");}
 else if(p.status==ContinuationStatus::cancelled)return bad("ContinuationCancellationSourceRequired",p.pending_id);
 return common_success();
}
Status NativeContinuationOriginalSourceRead::legacy_acceptance(const ConsentRef& ref,const StableId& predecessor,const StableId& before,const CommonActualFactPrefix* prefix)const{
 const auto& x=source_;const auto* a=x.legacy.consents.find(ref.accepted_fact_id);if(!a||a->kind!="family_legacy_control_consent"||a->source.decision!="accept"||a->source.predecessor_actor_id!=predecessor||!same_continuation_consent(ref,consent_ref(ref.accepted_fact_id,a->source)))return common_failure("ContinuationAcceptanceRequired");
 const auto* h=x.common.state().facts.find(ref.accepted_fact_id);if(!h||h->domain!="family_legacy"||h->kind!=a->kind||(!before.empty()&&!precedes(x.common,ref.accepted_fact_id,before)))return bad("ContinuationAcceptanceBoundaryMismatch",ref.accepted_fact_id);
 if(prefix){auto included=actual_common_fact_in_prefix(x.common,{"family_legacy",a->kind,ref.accepted_fact_id},*prefix);if(!included.ok)return {false,{},included.error};if(!included.value)return bad("ContinuationAcceptanceBoundaryMismatch",ref.accepted_fact_id);}
 auto valid=validate_legacy_control_decision_source(x.common,x.family,x.common_catalog,x.interactions,x.legacy_catalog,a->source,x.disciple,ref.accepted_fact_id,x.biological,x.collateral,&x.people);if(!valid.ok)return valid;
 auto controller=read_control_actor_before_original_fact(x.common,x.controls,x.family.founder_actor_id,{h->domain,h->kind,ref.accepted_fact_id});if(!controller.ok)return {false,{},controller.error};if(controller.value!=predecessor)return bad("ContinuationConsentControllerMismatch",ref.accepted_fact_id);
 for(const auto& r:x.legacy.revocations.rows())if(r.second.consent_fact_id==ref.accepted_fact_id){if(prefix){auto included=actual_common_fact_in_prefix(x.common,{"family_legacy",r.second.kind,r.first},*prefix);if(!included.ok)return {false,{},included.error};if(!included.value)continue;}else if(!before.empty()&&!precedes(x.common,r.first,before))continue;const auto* rh=x.common.state().facts.find(r.first);if(!rh||rh->domain!="family_legacy"||rh->kind!=r.second.kind)return bad("ContinuationWithdrawalSourceMismatch",r.first);valid=validate_legacy_control_decision_source(x.common,x.family,x.common_catalog,x.interactions,x.legacy_catalog,r.second.source,x.disciple,r.first,x.biological,x.collateral,&x.people);if(!valid.ok)return valid;return common_failure("ContinuationConsentWithdrawn");}
 return common_success();
}
Status NativeContinuationOriginalSourceRead::validate_consent_source(const ContinuationConsentSource& s)const{
 const auto& x=source_;if(s.original_source.domain!="family_legacy"||s.original_source.ordinal)return common_failure("ContinuationConsentOwnerRequired",{},ErrorKind::unsupported_capability);
 const LegacyControlDecisionSource* actual=nullptr;const auto* accepted=x.legacy.consents.find(s.consent_ref.accepted_fact_id);
 if(s.decision==ContinuationConsentSource::Decision::accept){if(s.original_source.kind!="family_legacy_control_consent"||s.original_source.fact_id!=s.consent_ref.accepted_fact_id||!accepted)return bad("InvalidContinuationConsentIndex");actual=&accepted->source;}
 else{const auto* r=x.legacy.revocations.find(s.original_source.fact_id);if(s.original_source.kind!="family_legacy_control_revocation"||!r||!accepted||r->consent_fact_id!=s.consent_ref.accepted_fact_id||!precedes(x.common,r->consent_fact_id,s.original_source.fact_id))return bad("InvalidContinuationWithdrawalIndex");actual=&r->source;}
 if(!same_continuation_consent(s.consent_ref,consent_ref(s.consent_ref.accepted_fact_id,accepted->source))||actual->predecessor_actor_id!=accepted->source.predecessor_actor_id||actual->successor_actor_id!=accepted->source.successor_actor_id)return bad("ContinuationConsentIndexMismatch");const auto* h=x.common.state().facts.find(s.original_source.fact_id);if(!h||h->domain!=s.original_source.domain||h->kind!=s.original_source.kind)return bad("ContinuationConsentOriginalSourceRequired");
 return validate_legacy_control_decision_source(x.common,x.family,x.common_catalog,x.interactions,x.legacy_catalog,*actual,x.disciple,s.original_source.fact_id,x.biological,x.collateral,&x.people);
}
Status NativeContinuationOriginalSourceRead::validate_original_consent_registry(const ControlContinuationState& state)const{
 const auto& x=source_;if(&state!=&x.controls||!bound_to(x.common))return bad("ContinuationCandidateMismatch");
 for(const auto& row:x.legacy.consents.rows()){const auto* saved=state.continuation_consent_sources.find(row.first);if(!saved||saved->decision!=ContinuationConsentSource::Decision::accept||!same_continuation_consent(saved->consent_ref,consent_ref(row.first,row.second.source)))return bad("MissingContinuationConsentReference",row.first);auto valid=validate_consent_source(*saved);if(!valid.ok)return valid;}
 for(const auto& row:x.legacy.revocations.rows()){const auto* saved=state.continuation_consent_sources.find(row.first);const auto* accepted=x.legacy.consents.find(row.second.consent_fact_id);if(!saved||!accepted||saved->decision!=ContinuationConsentSource::Decision::revoke||!same_continuation_consent(saved->consent_ref,consent_ref(row.second.consent_fact_id,accepted->source)))return bad("MissingContinuationWithdrawalReference",row.first);auto valid=validate_consent_source(*saved);if(!valid.ok)return valid;}
 return common_success();
}
Status NativeContinuationOriginalSourceRead::candidate(const PendingContinuation& pending,const StableId& successor,ContinuationPath chosen,const CauseRef& basis,const ConsentRef& acceptance,Integer tick,const StableId& before,const CommonActualFactPrefix* prefix,Integer source_revision)const{
 const auto& x=source_;if(prefix){auto included=actual_common_fact_in_prefix(x.common,{pending.source_ref.domain,pending.source_ref.kind,pending.source_ref.fact_id},*prefix);if(!included.ok)return {false,{},included.error};if(!before.empty()||!included.value)return bad("ContinuationPendingPrefixMismatch",pending.pending_id);}
 const auto* h=before.empty()?nullptr:x.common.state().facts.find(before);if(!before.empty()&&(!h||h->domain!="control_continuation"||h->kind!="control_activated"))return bad("ContinuationActivationBoundaryRequired",before);
 auto presence=prefix?x.people.read_presence_at_original_prefix(successor,tick,*prefix):h?x.people.read_presence_before_original_fact(successor,tick,{h->domain,h->kind,before}):x.people.read_current_presence(successor);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return common_failure("ContinuationCandidateDead");auto age=prefix?x.people.read_age_at_original_prefix(successor,tick,*prefix):h?x.people.read_age_before_original_fact(successor,tick,{h->domain,h->kind,before}):x.people.read_current_age(successor);if(!age.ok)return {false,{},age.error};if(age.value.age_years<18)return common_failure("ContinuationNotAdult");if(successor==pending.predecessor_actor_id||acceptance.decision_actor_id!=successor)return bad("ContinuationPartiesMismatch");
 if(pending.mode==ContinuationMode::voluntary_retirement){auto valid=validate_voluntary_preparation(pending);if(!valid.ok)return valid;const auto& plan=std::get<SuccessionConsentFact>(*x.family.facts.find(pending.source_ref.fact_id));const auto& a=plan.agreement;if(prefix&&plan.conclusion){auto withdrawal=read_original_pending_retirement_withdrawal(x.common,x.family,x.controls,pending.pending_id);if(!withdrawal.ok)return {false,{},withdrawal.error};if(source_revision<0||withdrawal.value.result->revision<=source_revision)return common_failure("ContinuationPreparationResolved");}if(acceptance.accepted_fact_id!=pending.source_ref.fact_id||acceptance.proposal_id!=a.request_id||acceptance.terms_version!=a.terms_version||successor!=a.personal.successor_actor_id)return common_failure("ContinuationAcceptanceRequired");auto relation=succession_adoption_basis(x.common,x.family,pending.predecessor_actor_id,successor,tick);if(!relation.ok)return {false,{},relation.error};auto original=actual_fact(x.common,relation.value);if(!original.ok)return {false,{},original.error};const auto expected=original.value.kind=="family_adult_adoption"?ContinuationPath::adult_adopted:ContinuationPath::adopted;if(chosen!=expected||!same_continuation_cause(basis,original.value))return common_failure("ContinuationBasisInvalid");if(prefix){auto included=actual_common_fact_in_prefix(x.common,{original.value.domain,original.value.kind,original.value.fact_id},*prefix);if(!included.ok)return {false,{},included.error};if(!included.value)return common_failure("ContinuationBasisInvalid");}auto pred=prefix?x.people.read_presence_at_original_prefix(pending.predecessor_actor_id,tick,*prefix):h?x.people.read_presence_before_original_fact(pending.predecessor_actor_id,tick,{h->domain,h->kind,before}):x.people.read_current_presence(pending.predecessor_actor_id);if(!pred.ok)return {false,{},pred.error};return pred.value.alive?common_success():common_failure("ContinuationLivingPredecessorRequired");}
 auto valid=legacy_acceptance(acceptance,pending.predecessor_actor_id,before,prefix);if(!valid.ok)return valid;const auto& a=x.legacy.consents.find(acceptance.accepted_fact_id)->source;auto actual_path=path(x,a);if(!actual_path.ok)return {false,{},actual_path.error};if(actual_path.value.first!=chosen||!same_continuation_cause(actual_path.value.second,basis))return common_failure("ContinuationBasisInvalid");auto death=read_death({pending.source_ref.domain,pending.source_ref.kind,pending.source_ref.fact_id});if(!death.ok)return {false,{},death.error};if(death.value.actor_id!=pending.predecessor_actor_id||!precedes(x.common,acceptance.accepted_fact_id,death.value.cause_ref.id))return common_failure("ContinuationAcceptanceAfterDeath");if(prefix){auto historical=x.people.life().read_actor_death_at_original_prefix(pending.predecessor_actor_id,tick,*prefix);if(!historical.ok)return {false,{},historical.error};if(!historical.value||historical.value->death_ref.id!=death.value.death_ref.id)return bad("ContinuationDeathPrefixMismatch",pending.pending_id);}else if(h){valid=x.people.life().require_death_before_original_fact(death.value.death_ref,pending.predecessor_actor_id,tick,{h->domain,h->kind,before});if(!valid.ok)return valid;}
 if(chosen==ContinuationPath::biological){if(!x.biological)return common_failure("ContinuationBiologicalSourceRequired",{},ErrorKind::unsupported_capability);auto b=prefix?read_biological_legacy_basis_at_original_prefix(x.biological->people,pending.predecessor_actor_id,successor,tick,*prefix):h?read_biological_legacy_basis_before_fact(x.biological->people,pending.predecessor_actor_id,successor,tick,before):read_biological_legacy_basis(x.biological->people,pending.predecessor_actor_id,successor);if(!b.ok)return {false,{},b.error};return b.value.death_fact_id==death.value.death_ref.id&&a.biological_origin&&same_biological(*a.biological_origin,b.value.relation)?common_success():common_failure("ContinuationBasisInvalid");}
 if(chosen==ContinuationPath::collateral){if(!x.collateral)return common_failure("ContinuationCollateralSourceRequired",{},ErrorKind::unsupported_capability);auto b=prefix?read_collateral_legacy_basis_at_original_prefix(*x.collateral,a.relation_fact_id,pending.predecessor_actor_id,successor,tick,*prefix):h?read_collateral_legacy_basis_before_fact(*x.collateral,a.relation_fact_id,pending.predecessor_actor_id,successor,tick,before):read_collateral_legacy_basis(*x.collateral,a.relation_fact_id,pending.predecessor_actor_id,successor);if(!b.ok)return {false,{},b.error};return b.value.death_fact_id==death.value.death_ref.id?common_success():common_failure("ContinuationBasisInvalid");}
 if(chosen==ContinuationPath::disciple){if(!x.disciple)return common_failure("ContinuationDiscipleSourceRequired",{},ErrorKind::unsupported_capability);ApprenticeshipLegacyBasisContext input{x.common,x.common_catalog,x.disciple->apprenticeships,x.deaths,x.disciple->people.births,{},&x.people};auto b=prefix?read_apprenticeship_legacy_basis_at_original_prefix(input,a.relation_fact_id,pending.predecessor_actor_id,successor,tick,*prefix):h?read_apprenticeship_legacy_basis_before_fact(input,a.relation_fact_id,pending.predecessor_actor_id,successor,tick,before):read_apprenticeship_legacy_basis(input,a.relation_fact_id,pending.predecessor_actor_id,successor);if(!b.ok)return {false,{},b.error};const auto* req=x.disciple->catalog.requests.find(a.request_id);return req&&req->skill_id==b.value.skill_id&&b.value.death_fact_id==death.value.death_ref.id?common_success():common_failure("ContinuationBasisInvalid");}
 auto relation=succession_adoption_basis(x.common,x.family,pending.predecessor_actor_id,successor,death.value.effective_at.exact_moment.half_day_tick);if(!relation.ok)return {false,{},relation.error};return relation.value==a.relation_fact_id&&precedes(x.common,relation.value,death.value.cause_ref.id)?common_success():common_failure("ContinuationBasisInvalid");
}
Status NativeContinuationOriginalSourceRead::validate_current_candidate(const PendingContinuation& p,const ContinuationCandidateRef& r)const{auto now=actual_now();if(!now.ok)return {false,{},now.error};return candidate(p,r.candidate_actor_id,r.path,r.basis_ref,r.acceptance_ref,now.value.half_day_tick,{});}
Status NativeContinuationOriginalSourceRead::validate_historical_activation(const PendingContinuation& pending,const ControlActivationRecord& activation)const{
 auto valid=candidate(pending,activation.successor_actor_id,activation.path,activation.basis_ref,activation.acceptance_ref,activation.effective_tick,activation.activation_id);if(!valid.ok)return valid;
 auto actual=read_common_command_source(source_.common,{"common","command_result",activation.command_result_ref});if(!actual.ok)return {false,{},actual.error};
 if(actual.value.request_seq!=activation.request_origin.request_seq||actual.value.history->actor_id!=activation.predecessor_actor_id||actual.value.result->tick!=activation.committed_tick||actual.value.result->revision!=activation.committed_revision||actual.value.result->kind!=activation.command_kind)return bad("InvalidControlActivationOrigin",activation.activation_id);
 return common_success();
}
Status NativeContinuationOriginalSourceRead::validate_historical_retirement_withdrawal(const PendingContinuation& pending)const{if(source_.controls.pending_continuations.find(pending.pending_id)!=&pending)return bad("ContinuationCandidateMismatch");auto actual=read_original_pending_retirement_withdrawal(source_.common,source_.family,source_.controls,pending.pending_id);return {actual.ok,{},actual.error};}
Status NativeContinuationOriginalSourceRead::validate_historical_boundary_candidate(const PendingContinuation& pending,const ContinuationBoundaryReceipt& receipt)const{
 if(!bound_to(source_.common))return bad("ContinuationCandidateMismatch");
 auto valid=validate_continuation_boundary_receipt_shape(receipt);if(!valid.ok)return valid;
 auto now=actual_now();if(!now.ok)return {false,{},now.error};auto time=compare_world_moments(receipt.actual_at,now.value);if(!time.ok)return {false,{},time.error};if(time.value>0)return bad("ContinuationBoundaryFutureMoment",pending.pending_id);
 const auto& chosen=receipt.request.boundary_ref.candidate;
 if(chosen.pending_id!=pending.pending_id||chosen.pending_generation>pending.generation||chosen.control_epoch!=pending.control_epoch_at_open)return bad("ContinuationBoundaryPendingLineageMismatch",pending.pending_id);
 auto generation=read_continuation_generation_at_revision(source_.common,source_.controls,pending.pending_id,chosen.source_identity.revision);if(!generation.ok)return {false,{},generation.error};if(generation.value!=chosen.pending_generation)return bad("ContinuationBoundaryGenerationPrefixMismatch",pending.pending_id);
 auto cut=read_actual_common_fact_prefix(source_.common,receipt.candidate_prefix);if(!cut.ok)return {false,{},cut.error};
 StableId controller=source_.family.founder_actor_id;Integer epoch=0;
 for(const auto& row:source_.controls.activation_records.rows()){auto included=actual_common_fact_in_prefix(source_.common,{"control_continuation","control_activated",row.first},receipt.candidate_prefix);if(!included.ok)return {false,{},included.error};if(!included.value)continue;const auto& activation=row.second;if(activation.predecessor_actor_id!=controller||activation.previous_control_epoch!=epoch||activation.committed_revision>chosen.source_identity.revision)return bad("ContinuationBoundaryControlPrefixMismatch",row.first);controller=activation.successor_actor_id;epoch=activation.new_control_epoch;}
 if(controller!=pending.predecessor_actor_id||epoch!=chosen.control_epoch)return bad("ContinuationBoundaryControlPrefixMismatch",pending.pending_id);
 return candidate(pending,chosen.candidate_actor_id,chosen.path,chosen.basis_ref,chosen.acceptance_ref,receipt.actual_at.half_day_tick,{},&receipt.candidate_prefix,chosen.source_identity.revision);
}
Outcome<ContinuationCandidateRef> NativeContinuationOriginalSourceRead::read_accepted_candidate(const PendingContinuation& p,const ConsentRef& accepted)const{
 const auto& x=source_;ContinuationCandidateRef out;out.pending_id=p.pending_id;out.pending_generation=p.generation;out.candidate_actor_id=accepted.decision_actor_id;out.acceptance_ref=accepted;out.source_identity=x.common.state().identity;out.control_epoch=p.control_epoch_at_open;
 if(p.mode==ContinuationMode::after_death){const auto* a=x.legacy.consents.find(accepted.accepted_fact_id);if(!a)return {false,{},common_failure("ContinuationAcceptanceRequired").error};auto selected=path(x,a->source);if(!selected.ok)return {false,{},selected.error};out.path=selected.value.first;out.basis_ref=std::move(selected.value.second);}else{auto rel=succession_adoption_basis(x.common,x.family,p.predecessor_actor_id,accepted.decision_actor_id,x.common.tick());if(!rel.ok)return {false,{},rel.error};auto src=actual_fact(x.common,rel.value);if(!src.ok)return {false,{},src.error};out.path=src.value.kind=="family_adult_adoption"?ContinuationPath::adult_adopted:ContinuationPath::adopted;out.basis_ref=std::move(src.value);}
 auto valid=validate_current_candidate(p,out);return {valid.ok,std::move(out),valid.error};
}
Outcome<StableId> read_control_actor_before_original_fact(const CommonStore& c,const ControlContinuationState& controls,const StableId& initial,const SourceRef& source){
 const auto* boundary=c.state().facts.find(source.id);if(!boundary||boundary->domain!=source.domain||boundary->kind!=source.kind)return {false,{},bad("ControlActualBoundaryRequired",source.id).error};
 if(initial.empty()||!c.actor(initial))return {false,{},bad("ContinuationInitialActorMismatch",initial).error};StableId actor=initial,result=initial,previous;Integer epoch=0;
 for(const auto& row:controls.activation_records.rows()){const auto& a=row.second;const auto* h=c.state().facts.find(row.first);
  if(row.first!=a.activation_id||!h||h->domain!="control_continuation"||h->kind!="control_activated"||epoch==std::numeric_limits<Integer>::max()||a.previous_control_epoch!=epoch||a.new_control_epoch!=epoch+1||a.predecessor_actor_id!=actor||a.successor_actor_id==actor||!c.actor(a.successor_actor_id)||(!previous.empty()&&!precedes(c,previous,row.first)))return {false,{},bad("InvalidControlActivationChain",row.first).error};
  actor=a.successor_actor_id;epoch=a.new_control_epoch;previous=row.first;if(precedes(c,row.first,source.id))result=actor;
 }
 return {true,std::move(result),{}};
}

bool NativeOriginalContinuationConsentWrite::bound_to(const CommonStore& c,const FamilyLegacyState& l,const ControlContinuationState& state)const noexcept{return &common_==&c&&&legacy_==&l&&&state_==&state&&sources_.bound_to(c);}
Status NativeOriginalContinuationConsentWrite::consume_original_legacy_decision(const StableId& id){
 if(!sources_.bound_to(common_))return bad("ContinuationCandidateMismatch");const auto* header=common_.state().facts.find(id);if(!header||header->domain!="family_legacy")return bad("ContinuationConsentOriginalSourceRequired",id);
 ContinuationConsentSource value;value.original_source={header->domain,header->kind,id,{}};
 if(header->kind=="family_legacy_control_consent"){const auto* row=legacy_.consents.find(id);if(!row||row->kind!=header->kind)return bad("ContinuationConsentOriginalSourceRequired",id);value.decision=ContinuationConsentSource::Decision::accept;value.consent_ref=consent_ref(id,row->source);}
 else if(header->kind=="family_legacy_control_revocation"){const auto* row=legacy_.revocations.find(id);const auto* accepted=row?legacy_.consents.find(row->consent_fact_id):nullptr;if(!row||!accepted||row->kind!=header->kind)return bad("ContinuationConsentOriginalSourceRequired",id);value.decision=ContinuationConsentSource::Decision::revoke;value.consent_ref=consent_ref(row->consent_fact_id,accepted->source);}
 else return bad("ContinuationConsentOriginalSourceRequired",id);
 return register_original_continuation_consent_in_candidate(common_,state_,sources_,value);
}

}
