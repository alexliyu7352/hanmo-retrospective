// 沿唯一Common分配序投影原家庭控制事实；不复制ControlBinding、不从卡列表猜玩家取得。
#include "profile_control_eligibility.hpp"
#include "profile_control_source.hpp"
#include "family_codec_version.hpp"
#include "control_continuation.hpp"
#include "care_handover_execution.hpp"
#include "voluntary_succession.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <limits>
namespace hanmo::resident {
namespace {
Outcome<Integer> sequence(const CommonState& c,const StableId& id,const StableId& prefix={}){auto colon=id.rfind(':');Integer n=0;if(colon==StableId::npos||colon==0||(!prefix.empty()&&id.substr(0,colon)!=prefix))return {false,{},common_failure("InvalidProfileControlInstance",id).error};const auto digits=id.substr(colon+1);auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||n<1||n>=c.next_instance_id||std::to_string(n)!=digits)return {false,{},common_failure("InvalidProfileControlInstance",id).error};return {true,n,{}};}
Outcome<ProfileControlEligibility> control(const ProfileControlEligibilitySources& s,Integer before){const auto& c=s.common;const auto& common=c.state();Integer actual_tick=c.tick();if(s.actual_clock){auto at=s.actual_clock->read_current_moment();if(!at.ok)return {false,{},at.error};actual_tick=at.value.half_day_tick;}Status valid=common_success();const auto& founder=s.family.founder_actor_id;if(founder.empty()||!c.actor(founder))return {false,{},common_failure("InvalidFamilyFounder").error};if(s.catalog.founder_selection!=FamilyFounderSelection::created_character){if(s.catalog.founder_selection!=FamilyFounderSelection::legacy_pair&&s.catalog.founder_selection!=FamilyFounderSelection::configured_roster)return {false,{},common_failure("InvalidFamilyFounder").error};if(s.catalog.allowed_founder_actor_ids.empty()?founder!=s.catalog.founder_actor_id:std::find(s.catalog.allowed_founder_actor_ids.begin(),s.catalog.allowed_founder_actor_ids.end(),founder)==s.catalog.allowed_founder_actor_ids.end())return {false,{},common_failure("InvalidFamilyFounder").error};}if(before<1||before>common.next_instance_id)return {false,{},common_failure("InvalidProfileControlBoundary").error};ProfileControlEligibility current;current.actor_id=s.family.founder_actor_id;current.source=AuthoredProfileController{common.metadata.scenario_id,current.actor_id};current.before_instance_sequence=before;
    if(s.catalog.founder_selection==FamilyFounderSelection::created_character){
        bool complete=false;if(s.foundation_catalog){std::size_t count=0;for(const auto& module:s.foundation_catalog->identity.module_versions)if(module.first=="actor_start_context"){++count;complete=module.second=="3";}if(count!=1)return {false,{},common_failure("ProfileControlOriginSelectionRequired").error};}
        if(complete){
            if(!s.foundation||!s.actual_clock)return {false,{},common_failure("ProfileControlCreatedOriginSourcesRequired").error};auto now=s.actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};auto created=read_created_character_origin_v2(c,*s.foundation,*s.foundation_catalog,s.catalog,s.shared,current.actor_id,now.value);if(!created.ok)return {false,{},created.error};
            // The actual actor_creation head is the activation event. The draft's
            // earlier allocation ID is not substituted for causal publication.
            auto number=sequence(common,current.actor_id,"actor");if(!number.ok||number.value>=before)return {false,{},common_failure("InvalidProfileControllerCreation",current.actor_id).error};current.source=CreatedProfileController{current.actor_id,current.actor_id};current.activation_sequence=number.value;
        }else{
valid=validate_character_creation_source_body(c,s.family,s.catalog,s.shared);if(!valid.ok)return {false,{},valid.error};bool found=false;for(const auto& row:s.family.facts.rows())if(const auto* created=std::get_if<CharacterCreationFact>(&row.second);created&&created->actor_id==current.actor_id){auto number=sequence(common,row.first);const auto* h=common.facts.find(row.first);if(found||!number.ok||number.value>=before||!h||h->domain!="family"||h->kind!="family_character_creation")return {false,{},common_failure("InvalidProfileControllerCreation",row.first).error};current.source=CreatedProfileController{row.first,current.actor_id};current.activation_sequence=number.value;found=true;}if(!found)return {false,{},common_failure("MissingProfileControllerCreation").error};
        }
    }
    if(s.control_continuation){
        const auto& controls=*s.control_continuation;if(!s.family.successions.empty()||(s.legacy&&!s.legacy->activations.empty()))return {false,{},common_failure("MixedControlContinuationChains",{},ErrorKind::internal).error};
        auto result=current;Integer epoch=0,prior_serial=current.activation_sequence,prior_tick=0;std::size_t actual_count=0;
        for(const auto& id:common.facts.order()){
            const auto* h=common.facts.find(id);const auto* a=controls.activation_records.find(id);if(!a){if(h&&h->domain=="control_continuation"&&h->kind=="control_activated")return {false,{},common_failure("MissingControlContinuationSource",id,ErrorKind::internal).error};continue;}
            const auto* pending=controls.pending_continuations.find(a->pending_id);auto n=sequence(common,id,"control_activation");
            if(!n.ok||!h||h->domain!="control_continuation"||h->kind!="control_activated"||a->activation_id!=id||!pending||pending->status!=ContinuationStatus::activated||pending->predecessor_actor_id!=a->predecessor_actor_id||pending->control_epoch_at_open!=epoch||!same_continuation_cause(pending->source_ref,a->source_ref)||pending->mode!=a->mode||a->predecessor_actor_id!=current.actor_id||a->successor_actor_id==current.actor_id||!c.actor(a->successor_actor_id)||epoch==std::numeric_limits<Integer>::max()||a->previous_control_epoch!=epoch||a->new_control_epoch!=epoch+1||a->effective_tick<prior_tick||a->effective_tick>actual_tick||n.value<=prior_serial||a->command_result_ref.empty()||a->command_kind!="select_continuation"||a->committed_tick!=a->effective_tick||a->committed_revision<1||a->committed_revision>c.revision()||a->request_origin.world_id!=common.identity.world_id||a->request_origin.execution_epoch_at_commit<1||a->request_origin.predecessor_actor_id!=a->predecessor_actor_id||a->request_origin.request_seq<1)return {false,{},common_failure("InvalidProfileControlActivation",id,ErrorKind::internal).error};
            current.actor_id=a->successor_actor_id;current.source=ContinuationProfileController{id,a->pending_id,a->successor_actor_id,a->new_control_epoch};current.activation_sequence=n.value;epoch=a->new_control_epoch;prior_serial=n.value;prior_tick=a->effective_tick;++actual_count;if(n.value<before)result=current;
        }
        if(actual_count!=controls.activation_records.size()||epoch!=controls.control_binding.control_epoch||current.actor_id!=controls.control_binding.actor_id||current.actor_id!=common.metadata.player_actor_id)return {false,{},common_failure("ProfileControlCurrentMismatch",{},ErrorKind::internal).error};return {true,std::move(result),{}};
    }
    auto result=current;std::size_t old_count=0,new_count=0;Integer previous_sequence=current.activation_sequence,previous_tick=0;
    for(const auto& id:common.facts.order()){FamilySuccessionStep step;bool changed=false;ProfileControllerSource next;const auto* h=common.facts.find(id);
        if(std::find(s.family.successions.begin(),s.family.successions.end(),id)!=s.family.successions.end()){auto original=family_succession_step(s.family,id);if(!original.ok||!h||h->domain!="family"||(h->kind!="family_succession"&&h->kind!="family_voluntary_succession")||old_count>=s.family.successions.size()||s.family.successions[old_count]!=id)return {false,{},common_failure("InvalidProfileSuccessionSource",id).error};step=original.value;next=SuccessionProfileController{id,step.successor_actor_id};++old_count;changed=true;}
        else if(s.legacy){if(const auto* activated=s.legacy->activations.find(id)){const auto* consent=s.legacy->consents.find(activated->consent_fact_id);if(!consent||!h||h->domain!="family_legacy"||h->kind!="family_legacy_control_activated"||activated->kind!=h->kind)return {false,{},common_failure("InvalidProfileLegacyActivation",id).error};auto consent_order=sequence(common,activated->consent_fact_id);auto death_order=sequence(common,activated->death_fact_id);auto activation_order=sequence(common,id);if(!consent_order.ok||!death_order.ok||!activation_order.ok||consent_order.value>=activation_order.value||death_order.value>=activation_order.value)return {false,{},common_failure("InvalidProfileLegacySourceOrder",id).error};step={consent->source.predecessor_actor_id,consent->source.successor_actor_id,activated->tick};next=LegacyProfileController{id,activated->consent_fact_id,step.successor_actor_id};++new_count;changed=true;}}
        if(!changed)continue;auto number=sequence(common,id);if(!number.ok||number.value<=previous_sequence||step.predecessor_actor_id!=current.actor_id||!c.actor(step.successor_actor_id)||step.successor_actor_id==s.family.founder_actor_id||step.tick<previous_tick||step.tick>actual_tick)return {false,{},common_failure("InvalidProfileControlChain",id).error};// 原控制链禁止回到已用人物；逐源扫描保持固定临时内存，不clone全链。
        for(const auto& previous:s.family.successions){auto prior_number=sequence(common,previous);if(!prior_number.ok)return {false,{},prior_number.error};if(prior_number.value>=number.value)continue;auto prior=family_succession_step(s.family,previous);if(!prior.ok)return {false,{},prior.error};if(prior.value.successor_actor_id==step.successor_actor_id)return {false,{},common_failure("RepeatedProfileController",id).error};}
        if(s.legacy)for(const auto& previous:s.legacy->activations.rows()){auto prior_number=sequence(common,previous.first);if(!prior_number.ok)return {false,{},prior_number.error};if(prior_number.value>=number.value)continue;const auto* consent=s.legacy->consents.find(previous.second.consent_fact_id);if(!consent)return {false,{},common_failure("InvalidProfileLegacyActivation",previous.first).error};if(consent->source.successor_actor_id==step.successor_actor_id)return {false,{},common_failure("RepeatedProfileController",id).error};}
        current.actor_id=step.successor_actor_id;current.source=std::move(next);current.activation_sequence=number.value;previous_sequence=number.value;previous_tick=step.tick;if(number.value<before)result=current;
    }
    if(old_count!=s.family.successions.size()||(s.legacy&&new_count!=s.legacy->activations.size())||current.actor_id!=common.metadata.player_actor_id)return {false,{},common_failure("ProfileControlCurrentMismatch").error};return {true,std::move(result),{}};
}
// 原解锁条件完成序来自真实settlement/report，而不是后建mapping的分配序。
Outcome<Integer> unlock_sequence(const CommonState& c,const GrowthActorUnlockEvidence& e){Integer latest=0;if(e.envoy){auto n=sequence(c,e.envoy->report_fact_id);if(!n.ok)return n;latest=n.value;}else {if(e.tasks.empty())return {false,{},common_failure("MissingActorUnlockCompletion").error};for(const auto& task:e.tasks){auto n=sequence(c,task.settlement_id,"settlement");if(!n.ok)return n;latest=std::max(latest,n.value);}}return {true,latest,{}};}
Outcome<ProfileAcquisitionEligibility> eligibility(const ProfileControlEligibilitySources& control_sources,const CardAcquisitionSources& sources,const CardAcquisitionState& state,const ProfileAcquisitionSource& ref,Integer before){if(&sources.common!=&control_sources.common.state())return {false,{},common_failure("ProfileControlCandidateMismatch").error};auto acquired=read_card_acquisition_v2(sources,state,ref);if(!acquired.ok)return {false,{},acquired.error};auto at=control(control_sources,before);if(!at.ok)return {false,{},at.error};if(acquired.value.canonical_identity.actor_id!=at.value.actor_id)return {false,{},common_failure("UncontrolledProfileAcquisition").error};const StableId id=std::holds_alternative<CardAcquisitionReceiptRefV2>(ref)?std::get<CardAcquisitionReceiptRefV2>(ref).receipt_id:std::get<LegacyCardSourceMappingRefV1>(ref).mapping_id;auto stored=sequence(sources.common,id);if(!stored.ok||stored.value>=before)return {false,{},common_failure("ProfileAcquisitionAfterDelivery").error};ProfileAcquisitionEligibility out;out.discovery_control=std::move(at.value);if(!acquired.value.actor_unlock_target)return {true,std::move(out),{}};
    const GrowthActorUnlockEvidence* evidence=nullptr;auto select=[&](const auto& basis){using T=std::decay_t<decltype(basis)>;if constexpr(std::is_same_v<T,ActorUnlockEvidenceV1>||std::is_same_v<T,ActorUnlockEvidenceV2>)evidence=&basis.evidence;};if(const auto* receipt=std::get_if<CardAcquisitionReceiptRefV2>(&ref))std::visit(select,state.card_acquisitions.find(receipt->receipt_id)->acquisition_basis);else std::visit(select,state.legacy_card_source_mappings.find(std::get<LegacyCardSourceMappingRefV1>(ref).mapping_id)->legacy_source);if(!evidence)return {false,{},common_failure("ActorUnlockEvidenceRequired").error};auto completion=unlock_sequence(sources.common,*evidence);if(!completion.ok)return {false,{},completion.error};if(completion.value>=before)return {false,{},common_failure("ActorUnlockAfterDelivery").error};auto then=control(control_sources,completion.value);if(!then.ok)return {false,{},then.error};out.actor_unlock_eligible=then.value.actor_id==evidence->earner_actor_id;return {true,std::move(out),{}};
}
}
Outcome<ProfileControlEligibility> read_current_profile_control(const ProfileControlEligibilitySources& s){return control(s,s.common.state().next_instance_id);}
Outcome<ProfileControllerActualSource> read_profile_controller_actual_source(
 const ProfileControlEligibilitySources& s,const CatalogIdentity& catalog,const SourceRef& ref){
 auto bad=[&](const char* code,ErrorKind kind=ErrorKind::internal)->Outcome<ProfileControllerActualSource>{return {false,{},common_failure(code,ref.id,kind).error};};
 const ProfileControllerSourceRegistration* registration=nullptr;
 for(const auto& entry:profile_controller_source_registrations)
  if(entry.domain==ref.domain&&entry.kind==ref.kind)registration=&entry;
 if(!registration)return bad("UnknownProfileControllerSource",ErrorKind::unsupported_capability);
 auto module=[&](const char* name)->Outcome<std::optional<Integer>>{
  const StableId* value=nullptr;for(const auto& entry:catalog.module_versions)if(entry.first==name){
   if(value)return {false,{},common_failure("DuplicateProfileControlModule",name,ErrorKind::internal).error};value=&entry.second;
  }
  if(!value)return {true,std::nullopt,{}};
  Integer n=0;const auto parsed=std::from_chars(value->data(),value->data()+value->size(),n);
  if(parsed.ec!=std::errc{}||parsed.ptr!=value->data()+value->size()||n<1||std::to_string(n)!=*value)
   return {false,{},common_failure("InvalidProfileControlModuleVersion",name,ErrorKind::internal).error};
  return {true,n,{}};
 };
 if(catalog.content_digest.empty())return bad("ProfileControlCatalogIdentityRequired",ErrorKind::unsupported_capability);
 if(s.foundation_catalog){const auto& actual=s.foundation_catalog->identity;
  if(actual.content_id!=catalog.content_id||actual.content_version!=catalog.content_version||actual.content_digest!=catalog.content_digest||actual.ruleset_id!=catalog.ruleset_id||actual.schema_sha256!=catalog.schema_sha256||actual.pack_set_digest!=catalog.pack_set_digest||actual.module_versions!=catalog.module_versions||actual.artifact_sha256!=catalog.artifact_sha256)
   return bad("ProfileControlCatalogIdentityMismatch");
 }
 const auto selected_family=module("family");if(!selected_family.ok)return {false,{},selected_family.error};
 const FamilyCodecSelection flags{bool(s.catalog.household_control),bool(s.catalog.character_creation),bool(s.catalog.adult_adoption),bool(s.catalog.voluntary_succession),s.catalog.voluntary_source_mode==FamilyVoluntarySourceMode::living_paths};
 const auto family_version=select_family_codec_version(flags);if(!family_version.ok)return {false,{},family_version.error};
 if(!s.catalog.enabled||selected_family.value!=std::optional<Integer>{family_version.value})return bad("ProfileControlFamilySelectionMismatch");
 const auto selected_control=module("control_continuation");if(!selected_control.ok)return {false,{},selected_control.error};
 if(bool(selected_control.value)!=bool(s.control_continuation)||(selected_control.value&&*selected_control.value!=2&&*selected_control.value!=3))return bad("ProfileControlContinuationSelectionMismatch");
 const auto selected_legacy=module("family_legacy");if(!selected_legacy.ok)return {false,{},selected_legacy.error};
 if(bool(selected_legacy.value)!=bool(s.legacy)||(selected_legacy.value&&(*selected_legacy.value<1||*selected_legacy.value>4)))return bad("ProfileControlLegacySelectionMismatch");
 const auto selected_start=module("actor_start_context");if(!selected_start.ok)return {false,{},selected_start.error};
 const auto& common=s.common.state();const auto* header=common.facts.find(ref.id);const auto& order=common.facts.order();
 if(!header||header->id!=ref.id||header->domain!=ref.domain||header->kind!=ref.kind||std::count(order.begin(),order.end(),ref.id)!=1)return bad("ProfileControlOriginalHeaderRequired");
 const StableId* actor=nullptr;
 if(ref.domain=="family"){
  if(ref.kind=="actor_creation"){
   if(selected_start.value!=std::optional<Integer>{3}||!s.foundation||!s.foundation_catalog||!s.actual_clock||s.catalog.founder_selection!=FamilyFounderSelection::created_character)return bad("ProfileControlCreatedOriginSourcesRequired",ErrorKind::unsupported_capability);
   const auto now=s.actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};
   const auto created=read_created_character_origin_v2(s.common,*s.foundation,*s.foundation_catalog,s.catalog,s.shared,ref.id,now.value);if(!created.ok)return {false,{},created.error};
   if(!created.value.registration||created.value.registration->actor_id!=s.family.founder_actor_id)return bad("ProfileControlCreatedFounderMismatch");actor=&created.value.registration->actor_id;
  }else{
   const auto* payload=s.family.facts.find(ref.id);if(!payload)return bad("ProfileControlOriginalPayloadRequired");
   if(ref.kind=="family_character_creation"){
    const auto* row=std::get_if<CharacterCreationFact>(payload);if(!row||row->kind!=ref.kind||selected_start.value==std::optional<Integer>{3}||s.catalog.founder_selection!=FamilyFounderSelection::created_character)return bad("ProfileControlLegacyCreationMismatch");actor=&row->actor_id;
   }else if(ref.kind=="family_succession"){
    const auto* row=std::get_if<FamilyChoice>(payload);if(!row||row->kind!=ref.kind)return bad("ProfileControlSuccessionPayloadMismatch");actor=&row->heir_actor_id;
   }else{
    const auto* row=std::get_if<VoluntarySuccessionFact>(payload);const auto* plan_value=row?s.family.facts.find(row->consent_fact_id):nullptr;const auto* plan=plan_value?std::get_if<SuccessionConsentFact>(plan_value):nullptr;
    if(!row||row->kind!=ref.kind||!plan||!s.catalog.voluntary_succession)return bad("ProfileControlVoluntaryPayloadMismatch");actor=&plan->agreement.personal.successor_actor_id;
   }
  }
 }else if(ref.domain=="family_legacy"){
  const auto* row=s.legacy?s.legacy->activations.find(ref.id):nullptr;const auto* consent=row?s.legacy->consents.find(row->consent_fact_id):nullptr;
  if(!row||row->kind!=ref.kind||!consent)return bad("ProfileControlLegacyPayloadMismatch");actor=&consent->source.successor_actor_id;
 }else{
  const auto* row=s.control_continuation?s.control_continuation->activation_records.find(ref.id):nullptr;
  if(!row||row->activation_id!=ref.id)return bad("ProfileControlActivationPayloadMismatch");actor=&row->successor_actor_id;
 }
 const auto serial=sequence(common,ref.id);if(!serial.ok)return {false,{},serial.error};
 // The source's own allocated serial is included. No caller-provided cut or
 // current-holder substitution can turn another payload into this adoption.
 const auto adopted=control(s,serial.value+1);if(!adopted.ok)return {false,{},adopted.error};
 bool matches=false;std::visit([&](const auto& original){using T=std::decay_t<decltype(original)>;
  if constexpr(std::is_same_v<T,CreatedProfileController>)matches=original.creation_fact_id==ref.id;
  else if constexpr(std::is_same_v<T,SuccessionProfileController>)matches=original.succession_fact_id==ref.id;
  else if constexpr(std::is_same_v<T,LegacyProfileController>)matches=original.activation_fact_id==ref.id;
  else if constexpr(std::is_same_v<T,ContinuationProfileController>)matches=original.activation_id==ref.id;
 },adopted.value.source);
 if(!matches||!actor||*actor!=adopted.value.actor_id)return bad("ProfileControlSourceNotActualAdoption");
 return {true,{header,actor,registration->source_schema},{}};
}
Outcome<ProfileControlEligibility> read_profile_control_before_fact(const ProfileControlEligibilitySources& s,const SourceRef& ref){const auto* h=s.common.state().facts.find(ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind)return {false,{},common_failure("ProfileControlFactBoundaryRequired",ref.id).error};auto before=sequence(s.common.state(),ref.id);if(!before.ok)return {false,{},before.error};return control(s,before.value);}
Outcome<ProfileControlEligibility> read_profile_control_before_growth_boundary(const ProfileControlEligibilitySources& s,const GrowthState& growth,const StableId& id){
 const auto* boundary=growth.fixed_service_boundaries.find(id);if(!boundary||boundary->boundary_id!=id||boundary->source_schema!=1||!s.actual_clock)return {false,{},common_failure("ProfileControlGrowthBoundaryRequired",id).error};
 auto current=s.actual_clock->read_current_moment();if(!current.ok)return {false,{},current.error};auto order=compare_world_moments(boundary->occurred_at,current.value);if(!order.ok)return {false,{},order.error};if(order.value>0)return {false,{},common_failure("ProfileControlGrowthBoundaryInFuture",id).error};
 auto before=sequence(s.common.state(),id,"growth_fixed_service_boundary");if(!before.ok)return {false,{},before.error};return control(s,before.value);
}
Outcome<ProfileControlEligibility> read_profile_control_before_care_handover_boundary(const ProfileControlEligibilitySources& s,const OrderedTable<CareHandoverExecution>& executions,const StableId& id){
 if(!s.actual_clock)return {false,{},common_failure("CareControlActualClockRequired",id,ErrorKind::unsupported_capability).error};
 const CareHandoverExecutionBoundary1* found=nullptr;
 for(const auto& pair:executions.rows())for(const auto& boundary:pair.second.handover_execution_boundaries)if(boundary.boundary_id==id){
  if(found||pair.first!=pair.second.handover_id||boundary.execution_ref!=pair.first)return {false,{},common_failure("CareControlBoundaryIdentityMismatch",id,ErrorKind::internal).error};
  auto valid=validate_care_handover_execution_shape(pair.second,5);if(!valid.ok)return {false,{},valid.error};found=&boundary;
 }
 if(!found)return {false,{},common_failure("CareControlOriginalBoundaryRequired",id,ErrorKind::internal).error};
 auto now=s.actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};auto order=compare_world_moments(found->occurred_at,now.value);if(!order.ok)return {false,{},order.error};if(order.value>0)return {false,{},common_failure("CareControlBoundaryInFuture",id,ErrorKind::internal).error};
 auto cut=sequence(s.common.state(),id,"care_handover_boundary");if(!cut.ok)return {false,{},cut.error};return control(s,cut.value);
}
Outcome<ProfileControlEligibility> read_profile_control_before_delivery(const ProfileControlEligibilitySources& s,const StableId& id){auto before=sequence(s.common.state(),id,"profile_delivery");if(!before.ok)return {false,{},before.error};return control(s,before.value);}
Outcome<ProfileAcquisitionEligibility> read_current_profile_acquisition_eligibility(const ProfileControlEligibilitySources& c,const CardAcquisitionSources& s,const CardAcquisitionState& state,const ProfileAcquisitionSource& ref){return eligibility(c,s,state,ref,c.common.state().next_instance_id);}
Outcome<ProfileAcquisitionEligibility> read_profile_acquisition_delivery_eligibility(const ProfileControlEligibilitySources& c,const CardAcquisitionSources& s,const CardAcquisitionState& state,const ProfileAcquisitionSource& ref,const StableId& id){auto before=sequence(c.common.state(),id,"profile_delivery");if(!before.ok)return {false,{},before.error};return eligibility(c,s,state,ref,before.value);}
}
