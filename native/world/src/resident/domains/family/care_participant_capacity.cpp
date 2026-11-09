#include "care_participant_capacity.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
namespace hanmo::resident { namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
}
Outcome<const CharacterPurposeBinding*> read_original_care_participant_purpose(const ActorFoundationCatalog& catalog){try{
 bool care=false;for(const auto& module:catalog.identity.module_versions)if(module.first=="care_requirements"){if(care||(module.second!="5"&&module.second!="6"))fail("CareParticipationSelectedSourcesRequired",{},ErrorKind::unsupported_capability);care=true;}
 if(!care||!condition_purpose_capacity_selected(catalog))fail("CareParticipationSelectedSourcesRequired",{},ErrorKind::unsupported_capability);
 take(validate_character_capability_catalog(catalog));const auto* binding=catalog.capability_purpose_bindings.find("care_participant");
 if(!binding||binding->reducer!=CharacterPurposeBinding::Reducer::family_care||binding->rule_ref.id!="original_family_care_alive")fail("CareParticipationPurposeRequired",{},ErrorKind::unsupported_capability);
 return {true,binding,{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CharacterCapabilityView> read_care_participant_capacity_at_head(const CharacterConditionSources& body,const ActorConditionHead& head,const WorldMoment& at){
 auto purpose=read_original_care_participant_purpose(body.catalog);if(!purpose.ok)return {false,{},purpose.error};return read_character_capability_at_head(body,head,purpose.value->purpose_ref,std::nullopt,at);
}
Outcome<CharacterCapabilityView> read_current_care_participant_capacity(const CharacterConditionSources& body,const StableId& actor,const ReadStamp& stamp,const ReadStampAuthority& authority,const DomainReadVersionSource& versions){
 auto purpose=read_original_care_participant_purpose(body.catalog);if(!purpose.ok)return {false,{},purpose.error};return read_character_capability(body,actor,purpose.value->purpose_ref,std::nullopt,stamp,authority,versions);
}
Status visit_care_participant_capacity_span(const CharacterConditionSources& body,const ActorConditionHead& head,const MomentSpan& span,CharacterCapabilitySpanVisitor& visitor){
 auto purpose=read_original_care_participant_purpose(body.catalog);if(!purpose.ok)return {false,{},purpose.error};return visit_character_capability_span(body,head,purpose.value->purpose_ref,std::nullopt,span,visitor);
}
Outcome<CareParticipantCapacityChange> read_care_participant_capacity_change(const CharacterConditionSources& body,const SourceRef& ref){try{
 const auto* purpose=take(read_original_care_participant_purpose(body.catalog));const auto* transition=take(read_verified_condition_purpose_capacity_transition(body,ref,2));
 if(!actor_foundation_rule_equal(transition->purpose,purpose->purpose_ref))fail("CareParticipationCapacityPurposeMismatch",ref.id);
 const auto* cause=std::get_if<ConditionPurposeCapacityCause>(&transition->cause_ref);if(!cause)fail("CareParticipationActualConditionRequired",ref.id);
 const auto* receipt=take(read_character_condition_receipt_provenance(body,cause->condition_receipt_ref));
 // No original resurrection or injury/fatigue care gate is registered here.
 // Do not interpret missing recovery time as permanent inability.
 if(!transition->from||transition->to||!receipt->life_transition_ref)fail("CareParticipationOriginalLifeTransitionRequired",ref.id,ErrorKind::unsupported_capability);
 return {true,{transition,receipt,&*receipt->life_transition_ref},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}

namespace hanmo::resident {
Status validate_care_participants_at_actual_handover(const CharacterConditionSources& body,const StableId& child,const StableId& carer,const SourceRef& placement,const WorldMoment& at,const CurrentMomentRead& clock){try{
 take(read_original_care_participant_purpose(body.catalog));if(child==carer)fail("CareDistinctActualParticipantsRequired",child);
 const auto dependent=take(read_character_availability_before_fact_provenance_v2(body,child,placement,at,clock));
 const auto caregiver=take(read_character_availability_before_fact_provenance_v2(body,carer,placement,at,clock));
 if(!dependent.present||!dependent.alive||!caregiver.present||!caregiver.alive||!caregiver.available)fail("CareHandoverActualParticipantsUnavailable",carer,ErrorKind::rule);
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_current_care_participants(const CharacterConditionSources& body,const StableId& child,const StableId& carer,const WorldMoment& at){try{
 take(read_original_care_participant_purpose(body.catalog));if(child==carer||take(compare_world_moments(at,body.actual_now)))fail("CareCurrentParticipantMomentMismatch",child);
 const auto dependent=take(read_current_character_condition(body,child));const auto caregiver=take(read_current_character_condition(body,carer));take(validate_military_availability_coverage(body));
 const auto* actor=body.common.actor(carer);if(!dependent.body.alive||!caregiver.body.alive||!actor||!actor->available)fail("CareImmediateParticipantLossMissing",carer);
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_participants_full_span_after_handover(const CharacterConditionSources& body,const StableId& child,const StableId& carer,const SourceRef& completed,const WorldMoment& started,const MomentSpan& span,const CurrentMomentRead& clock){try{
 take(read_original_care_participant_purpose(body.catalog));if(child==carer)fail("CareDistinctActualParticipantsRequired",child);
 const auto dependent=take(read_character_availability_span_after_fact_v2(body,child,completed,started,span,clock));
 const auto caregiver=take(read_character_availability_span_after_fact_v2(body,carer,completed,started,span,clock));
 if(!dependent.alive_for_full_span||!caregiver.alive_for_full_span||!caregiver.available_for_full_span)fail("CareOriginalParticipantSpanUnavailable",carer);
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
