#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "military_authority_time.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "resident/model/common_model.hpp"
#include "resident/domains/interaction/interaction_role_history.hpp"
#include <string_view>
#include <utility>
#include "resident/checkpoint/projection_writer.hpp"
#include "occupation_time.hpp"
#include "military_integration_assets.hpp"
#include "military_roster_history.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
SourceRefBuildView borrowed(const SourceRef& ref){return {ref.domain,ref.kind,ref.id};}
MomentOwnerSourceBuildView borrowed(const MomentOwnerSource& ref){return {borrowed(ref.source_ref),ref.source_schema};}
CheckpointMemoryCharge reserve(const MilitaryCampaignAcceptanceBuildView& input){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryOfficeResignationBuildView& input){return CheckpointMemoryCharge(0);}
void swap_source(SourceRef& a,SourceRef& b)noexcept{a.domain.swap(b.domain);a.kind.swap(b.kind);a.id.swap(b.id);}
void swap_source(MomentOwnerSource& a,MomentOwnerSource& b)noexcept{swap_source(a.source_ref,b.source_ref);std::swap(a.source_schema,b.source_schema);}
void swap_moment(WorldMoment& a,WorldMoment& b)noexcept{std::swap(a.half_day_tick,b.half_day_tick);a.sub_half_day.swap(b.sub_half_day);}
void swap_context(InteractionContext& a,InteractionContext& b)noexcept{
 a.place_id.swap(b.place_id);a.provider_actor_id.swap(b.provider_actor_id);a.role_id.swap(b.role_id);
 a.kind.swap(b.kind);a.encounter_id.swap(b.encounter_id);a.input_error.swap(b.input_error);a.field_order.swap(b.field_order);
}
template<class T>const T* unique(const std::vector<T>& rows,const StableId& id){
 const T* out=nullptr;for(const auto& row:rows)if(row.id==id){if(out)fail("MilitaryAuthorityOriginalOwnerAmbiguous",id);out=&row;}return out;
}
const MilitaryAuthorityTimeContext& bound(const CommonRead& common,const MilitaryState& state,const MilitaryAccess& access){
 if(access.military_authority_time_schema!=1||!access.authority_time)fail("MilitaryAuthorityTimeContextRequired");
 const auto& x=*access.authority_time;
 if(static_cast<const CommonRead*>(&x.common)!=&common||&x.military!=&state||!x.office.original.bound_to(x.common,state,x.actions))fail("CrossCandidateMilitaryAuthorityTime");
 return x;
}
const MilitaryActualSourceWindow* authority_window(const MilitaryAuthorityTimeContext&)noexcept{return nullptr;}
const MilitaryActualSourceWindow* authority_window(const MilitaryAuthoritySourceContext& x)noexcept{return x.window;}
template<class Context>void validate_original_authority_participants(
    const Context& x,const StableId& actor,
    const InteractionContext& context,const SourceRef& boundary,const WorldMoment& at){
    if(&x.office.body.common!=&x.common||
       (x.office.body.military&&x.office.body.military!=&x.military))
        fail("MilitaryAuthorityBodySourceRootMismatch",boundary.id);
    const auto check=[&](const StableId& participant){
        const auto available=take(read_character_availability_before_fact_provenance_v2(
            x.office.body,participant,boundary,at,x.office.clock));
        if(!available.present||!available.alive||!available.available)
            fail("MilitaryAuthorityOriginalParticipantAvailabilityMismatch",participant);
    };
    check(actor);
    check(context.provider_actor_id);
}
const DynamicMilitaryActionFact2& appointment(const MilitaryAuthorityTimeContext& x,const StableId& actor,const StableId& office){
 const auto* current=unique(x.military.offices,office);
 if(!current||!current->enabled||current->holder_actor_id!=actor)fail("MilitaryAuthorityCurrentOfficeRequired",office);
 const auto* action=x.actions.dynamic_action_facts.find(current->appointment_id);
 if(!action||!action->terminal_source_ref)fail("MilitaryAuthorityExactAppointmentRequired",office);
 const auto* verified=take(read_exact_office_completion_application(x.common,x.military,x.actions,x.elapsed,x.office,*action->terminal_source_ref,2));
 const auto* terms=std::get_if<OfficeTerms>(&verified->original_terms);
 if(!verified->success||!terms||terms->office_id!=office||terms->actor_id!=actor||!verified->result||verified->result->office_id!=office)fail("MilitaryAuthorityAppointmentMismatch",office);
 return *verified;
}
template<class Context>void source_clock(const Context& x,const WorldMoment& at,Integer legacy_tick,const SourceRef& source,std::string_view kind){
 if(source.domain!="military"||source.kind!=kind)fail("MilitaryAuthorityActualSourceRequired",source.id);
 take(read_actual_common_fact_position(x.common,source));take(encode_world_moment(at,x.office.budget));
 if(at.half_day_tick!=legacy_tick||take(compare_world_moments(at,take(x.office.clock.read_current_moment()),x.office.budget))>0)fail("MilitaryAuthorityActualClockMismatch",source.id);
 if(const auto* window=authority_window(x))take(validate_military_source_in_actual_window(x.common,source,at,take(x.office.clock.read_current_moment()),window,x.office.budget));
}
template<class Context>const DynamicMilitaryActionFact2& source_appointment(const Context& x,const MomentOwnerSource& source,const SourceRef& boundary,const WorldMoment& at,const StableId& actor,const StableId& office){
 const auto* action=take(read_exact_office_completion_application(x.common,x.military,x.actions,x.elapsed,x.office,source.source_ref,source.source_schema));
 const auto* terms=std::get_if<OfficeTerms>(&action->original_terms);
 if(!action->success||!terms||terms->actor_id!=actor||terms->office_id!=office||!action->result||action->result->office_id!=office||!action->completed_at)fail("MilitaryAuthorityAppointmentMismatch",office);
 if(take(read_actual_common_fact_position(x.common,source.source_ref))>=take(read_actual_common_fact_position(x.common,boundary))||take(compare_world_moments(*action->completed_at,at,x.office.budget))>0)fail("MilitaryAuthorityAppointmentOrderMismatch",boundary.id);
 if(const auto* window=authority_window(x))take(validate_military_source_in_actual_window(x.common,source.source_ref,*action->completed_at,take(x.office.clock.read_current_moment()),window,x.office.budget));
 return *action;
}
}
template<class Context>Outcome<OriginalMilitaryInteractionRoleBeforePrefix> validate_military_original_role_before_source_core(const CommonStore& common,const Context& x,const FactRecord& source,const InteractionContext& original_context,std::string_view expected_service,CommonActualFactPrefixSink* retained_prefix){
    const auto* definition=x.prepared.common
        ?x.prepared.common->roles.find(original_context.role_id):nullptr;
    const StableId* retained_service=nullptr;
    if(definition)for(const auto& service:definition->services)
        if(service==expected_service){retained_service=&service;break;}
    if(!retained_service)
        return {false,{},common_failure("MilitaryInteractionOriginalRoleServiceRequired",source.id).error};
    auto prefix=read_actual_common_fact_prefix_before(common,source);
    if(!prefix.ok)return {false,{},std::move(prefix.error)};
    auto role=read_original_military_interaction_role_before_prefix(
        common,x.prepared,x.organization,x.family,prefix.value,
        original_context.role_id,original_context.provider_actor_id,
        *retained_service,retained_prefix);
    if(!role.ok)return {false,{},std::move(role.error)};
    if(role.value.authored_role->place_id!=original_context.place_id)
        return {false,{},common_failure("MilitaryInteractionOriginalRolePlaceMismatch",source.id).error};
    return role;
}
Outcome<OriginalMilitaryInteractionRoleBeforePrefix> validate_military_original_role_before_source(const CommonStore& common,const ExactOfficeReadContext& x,const FactRecord& source,const InteractionContext& original_context,std::string_view expected_service,CommonActualFactPrefixSink* retained_prefix){return validate_military_original_role_before_source_core(common,x,source,original_context,expected_service,retained_prefix);}
Outcome<OriginalMilitaryInteractionRoleBeforePrefix> validate_military_original_role_before_source(const CommonStore& common,const ExactOfficeSourceReadContext& x,const FactRecord& source,const InteractionContext& original_context,std::string_view expected_service,CommonActualFactPrefixSink* retained_prefix){if(x.selected_action_time!=2||&x.common!=&common)return {false,{},common_failure("CrossCandidateExactOfficeSourceRoots").error};return validate_military_original_role_before_source_core(common,x,source,original_context,expected_service,retained_prefix);}
Status validate_military_authority_source_context(const MilitaryAuthoritySourceContext& x){return status([&]{
 if(x.selected_authority_time!=1||x.office.access.military_authority_time_schema!=1||x.office.selected_action_time!=2||&x.office.common!=&x.common||&x.office.military!=&x.military||&x.office.actions!=&x.actions||&x.office.elapsed!=&x.elapsed)fail("CrossCandidateMilitaryAuthoritySourceRoots");
 if(&x.office.body.common!=&x.common||(x.office.body.military&&x.office.body.military!=&x.military))fail("MilitaryAuthorityBodySourceRootMismatch");
});}
MilitaryCampaignAcceptanceTime1::MilitaryCampaignAcceptanceTime1(const MilitaryCampaignAcceptanceBuildView& v)
 :copied_fields(reserve(v)),accepted_at(v.accepted_at),
 actual_acceptance_source{StableId(v.actual_acceptance_source.domain),StableId(v.actual_acceptance_source.kind),StableId(v.actual_acceptance_source.id)},
 original_context(v.original_context),office_appointment_source{{StableId(v.office_appointment_source.source_ref.domain),StableId(v.office_appointment_source.source_ref.kind),StableId(v.office_appointment_source.source_ref.id)},v.office_appointment_source.source_schema}{}
MilitaryCampaignAcceptanceTime1::MilitaryCampaignAcceptanceTime1(const MilitaryCampaignAcceptanceTime1& v)
 :MilitaryCampaignAcceptanceTime1(MilitaryCampaignAcceptanceBuildView{v.accepted_at,borrowed(v.actual_acceptance_source),v.original_context,borrowed(v.office_appointment_source)}){}
void MilitaryCampaignAcceptanceTime1::swap(MilitaryCampaignAcceptanceTime1& v)noexcept{
 copied_fields.swap(v.copied_fields);swap_moment(accepted_at,v.accepted_at);swap_source(actual_acceptance_source,v.actual_acceptance_source);swap_context(original_context,v.original_context);swap_source(office_appointment_source,v.office_appointment_source);
}
MilitaryOfficeResignationTime1::MilitaryOfficeResignationTime1(const MilitaryOfficeResignationBuildView& v)
 :copied_fields(reserve(v)),resigned_at(v.resigned_at),
 actual_resignation_source{StableId(v.actual_resignation_source.domain),StableId(v.actual_resignation_source.kind),StableId(v.actual_resignation_source.id)},
 original_terms(v.original_terms),original_context(v.original_context),
 office_appointment_source{{StableId(v.office_appointment_source.source_ref.domain),StableId(v.office_appointment_source.source_ref.kind),StableId(v.office_appointment_source.source_ref.id)},v.office_appointment_source.source_schema}{}
MilitaryOfficeResignationTime1::MilitaryOfficeResignationTime1(const MilitaryOfficeResignationTime1& v)
 :MilitaryOfficeResignationTime1(MilitaryOfficeResignationBuildView{v.resigned_at,borrowed(v.actual_resignation_source),v.original_terms,v.original_context,borrowed(v.office_appointment_source)}){}
void MilitaryOfficeResignationTime1::swap(MilitaryOfficeResignationTime1& v)noexcept{
 copied_fields.swap(v.copied_fields);swap_moment(resigned_at,v.resigned_at);swap_source(actual_resignation_source,v.actual_resignation_source);
 original_terms.office_id.swap(v.original_terms.office_id);original_terms.actor_id.swap(v.original_terms.actor_id);original_terms.organization_id.swap(v.original_terms.organization_id);original_terms.place_id.swap(v.original_terms.place_id);original_terms.provider_actor_id.swap(v.original_terms.provider_actor_id);original_terms.field_order.swap(v.original_terms.field_order);
 swap_context(original_context,v.original_context);swap_source(office_appointment_source,v.office_appointment_source);
}
Status validate_military_authority_time_write(const CommonRead& common,const MilitaryState& state,const MilitaryAccess& access){return status([&]{
 if(access.military_authority_time_schema==0){if(access.authority_time)fail("UnexpectedMilitaryAuthorityTimeContext");return;}
 const auto& x=bound(common,state,access);const auto now=take(x.office.clock.read_current_moment());take(encode_world_moment(now,x.office.budget));
 if(now.half_day_tick!=common.tick())fail("MilitaryAuthorityActualClockMismatch");
});}
Outcome<std::optional<MilitaryCampaignAcceptanceTime1>> capture_military_campaign_acceptance(const CommonRead& common,const MilitaryState& state,const MilitaryAccess& access,const StableId& id,const StableId& actor,const StableId& office,const InteractionContext& context){return guard<std::optional<MilitaryCampaignAcceptanceTime1>>([&]()->std::optional<MilitaryCampaignAcceptanceTime1>{
 take(validate_military_authority_time_write(common,state,access));if(access.military_authority_time_schema==0)return std::nullopt;
 const auto& x=bound(common,state,access);if(x.common.state().facts.find(id)||unique(state.campaigns,id))fail("MilitaryCampaignAcceptanceAlreadyApplied",id);
 const auto& original=appointment(x,actor,office);const auto now=take(x.office.clock.read_current_moment());
 if(context.input_error||context.provider_actor_id.empty()||context.provider_actor_id==actor)fail("MilitaryCampaignAcceptanceContextRequired",id);
 return MilitaryCampaignAcceptanceTime1{MilitaryCampaignAcceptanceBuildView{now,{"military","campaign_accepted",id},context,{borrowed(*original.terminal_source_ref),2}}};
});}
Outcome<std::optional<MilitaryOfficeResignationTime1>> capture_military_office_resignation(const CommonRead& common,const MilitaryState& state,const MilitaryAccess& access,const StableId& id,const OfficeTerms& terms,const InteractionContext& context){return guard<std::optional<MilitaryOfficeResignationTime1>>([&]()->std::optional<MilitaryOfficeResignationTime1>{
 take(validate_military_authority_time_write(common,state,access));if(access.military_authority_time_schema==0)return std::nullopt;
 const auto& x=bound(common,state,access);if(x.common.state().facts.find(id)||unique(state.resignation_facts,id))fail("MilitaryOfficeResignationAlreadyApplied",id);
 const auto& original=appointment(x,terms.actor_id,terms.office_id);const auto now=take(x.office.clock.read_current_moment());
 if(context.input_error||context.provider_actor_id!=terms.provider_actor_id||context.place_id!=terms.place_id||terms.provider_actor_id.empty()||terms.provider_actor_id==terms.actor_id)fail("MilitaryOfficeResignationContextRequired",id);
 return MilitaryOfficeResignationTime1{MilitaryOfficeResignationBuildView{now,{"military","office_resignation",id},terms,context,{borrowed(*original.terminal_source_ref),2}}};
});}
template<class Context>Outcome<const CampaignRecord*> read_military_campaign_acceptance_source_core(const Context& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){auto verified=guard<const CampaignRecord*>([&]{
 if(schema!=1||ref.domain!="military"||ref.kind!="campaign_accepted")fail("MilitaryCampaignAcceptanceSourceRequired",ref.id);
 const auto* row=unique(x.military.campaigns,ref.id);if(!row||!row->exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",ref.id);const auto& time=*row->exact_acceptance;
 if(!same_source(ref,time.actual_acceptance_source))fail("MilitaryCampaignAcceptanceSourceMismatch",ref.id);
 source_clock(x,time.accepted_at,row->accepted_tick,ref,"campaign_accepted");
 const auto* definition=unique(x.office.catalog.orders,row->template_id);const auto* office=unique(x.office.catalog.offices,row->office_id);
 if(!definition||!office||definition->office_id!=row->office_id||definition->garrison_id!=row->garrison_id||definition->issuer_role_id!=row->issuer_role_id||office->organization_id!=row->issuer_organization_id||time.original_context.role_id!=row->issuer_role_id||time.original_context.input_error||time.original_context.provider_actor_id.empty()||time.original_context.provider_actor_id==row->actor_id)fail("MilitaryCampaignAcceptanceOriginalTermsMismatch",ref.id);
 source_appointment(x,time.office_appointment_source,ref,time.accepted_at,row->actor_id,row->office_id);return row;
});
    if(!verified.ok)return verified;
    auto role=validate_military_original_role_before_source(
        x.common,x.office,*x.common.state().facts.find(ref.id),
        verified.value->exact_acceptance->original_context,
        "military_order",retained_prefix);
    if(!role.ok)return {false,{},std::move(role.error)};
    if(role_output)*role_output=role.value;
    return verified;
}
Outcome<const CampaignRecord*> read_military_campaign_acceptance_source(const MilitaryAuthorityTimeContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){return read_military_campaign_acceptance_source_core(x,ref,schema,retained_prefix,role_output);}
Outcome<const CampaignRecord*> read_military_campaign_acceptance_source(const MilitaryAuthoritySourceContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){auto bound=validate_military_authority_source_context(x);if(!bound.ok)return {false,{},std::move(bound.error)};return read_military_campaign_acceptance_source_core(x,ref,schema,retained_prefix,role_output);}
template<class Context>Outcome<const MilitaryResignationFact*> read_military_office_resignation_source_core(const Context& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){auto verified=guard<const MilitaryResignationFact*>([&]{
 if(schema!=1||ref.domain!="military"||ref.kind!="office_resignation")fail("MilitaryOfficeResignationSourceRequired",ref.id);
 const auto* row=unique(x.military.resignation_facts,ref.id);if(!row||!row->exact_resignation)fail("MilitaryOfficeResignationSourceRequired",ref.id);const auto& time=*row->exact_resignation;const auto& terms=time.original_terms;
 if(!same_source(ref,time.actual_resignation_source))fail("MilitaryOfficeResignationSourceMismatch",ref.id);
 source_clock(x,time.resigned_at,row->tick,ref,"office_resignation");const auto* definition=unique(x.office.catalog.offices,row->office_id);
 if(!definition||terms.office_id!=row->office_id||terms.actor_id!=row->actor_id||terms.organization_id!=definition->organization_id||time.original_context.role_id!=definition->appointing_role_id||time.original_context.provider_actor_id!=terms.provider_actor_id||time.original_context.place_id!=terms.place_id||time.original_context.input_error||terms.provider_actor_id.empty()||terms.provider_actor_id==row->actor_id)fail("MilitaryOfficeResignationOriginalTermsMismatch",ref.id);
 source_appointment(x,time.office_appointment_source,ref,time.resigned_at,row->actor_id,row->office_id);return row;
});
    if(!verified.ok)return verified;
    auto role=validate_military_original_role_before_source(
        x.common,x.office,*x.common.state().facts.find(ref.id),
        verified.value->exact_resignation->original_context,
        "office_resign",retained_prefix);
    if(!role.ok)return {false,{},std::move(role.error)};
    if(role_output)*role_output=role.value;
    return verified;
}
Outcome<const MilitaryResignationFact*> read_military_office_resignation_source(const MilitaryAuthorityTimeContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){return read_military_office_resignation_source_core(x,ref,schema,retained_prefix,role_output);}
Outcome<const MilitaryResignationFact*> read_military_office_resignation_source(const MilitaryAuthoritySourceContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,OriginalMilitaryInteractionRoleBeforePrefix* role_output){auto bound=validate_military_authority_source_context(x);if(!bound.ok)return {false,{},std::move(bound.error)};return read_military_office_resignation_source_core(x,ref,schema,retained_prefix,role_output);}
template<class Context>Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_before_core(const Context& x,const StableId& office,const SourceRef* boundary,const CommonActualFactPrefix* prefix,const WorldMoment& at,const MilitaryActualSourceWindow* window=nullptr){return guard<MilitaryOfficeAuthorityBeforeSource>([&]{
 const auto* definition=unique(x.office.catalog.offices,office);if(!definition)fail("UnknownOffice",office);
 if(bool(boundary)==bool(prefix))fail("MilitaryAuthorityBoundaryModeRequired",office);
 const bool ordinal_only=prefix!=nullptr;
 const auto cut=ordinal_only?take(read_actual_common_fact_prefix(x.common,*prefix)):take(read_actual_common_fact_position(x.common,*boundary));
 const auto now=take(x.office.clock.read_current_moment());
 take(encode_world_moment(at,x.office.budget));
 if(ordinal_only){if(take(compare_world_moments(at,now,x.office.budget))!=0)fail("MilitaryAuthorityActualClockMismatch",office);take(validate_military_actual_source_window(x.common,*prefix,now,window,x.office.budget));}
 else {
  if(take(compare_world_moments(at,now,x.office.budget))>0)fail("MilitaryAuthorityFutureBoundary",boundary->id);
  if(window){const auto outer=take(read_actual_common_fact_prefix(x.common,window->outer_prefix));if(cut>outer)fail("MilitaryAuthorityOuterPrefixMismatch",office);take(encode_world_moment(window->at,x.office.budget));if(take(compare_world_moments(window->at,now,x.office.budget))>0)fail("MilitaryAuthorityFutureBoundary",office);}
 }
 struct Event {std::size_t position=0;const DynamicMilitaryActionFact2* appointment=nullptr;const MilitaryResignationFact* resignation=nullptr;const MilitaryOccupationFact* occupation=nullptr;const MilitaryIntegrationFact* integration=nullptr;};
 CheckpointVector<Event> events;
 const auto exact_event=[&](const auto& source,const WorldMoment& occurred,Event event,const StableId& original_id){
  const auto position=take(read_actual_common_fact_position(x.common,source));
  if(ordinal_only)take(validate_military_source_in_actual_window(x.common,SourceRefBuildView{source.domain,source.kind,source.id},occurred,now,window,x.office.budget));
  else {const auto order=take(compare_world_moments(occurred,at,x.office.budget));
   if((position<cut&&order>0)||(position>=cut&&order<0))fail("MilitaryOfficeHistoryClockOrderMismatch",original_id);
   if(window)take(validate_military_source_in_actual_window(x.common,SourceRefBuildView{source.domain,source.kind,source.id},occurred,now,window,x.office.budget));
  }
  if(position<cut){event.position=position;events.push_back(event);}
 };
 // Preserve the old selected exact proof boundary: a legacy completion for
 // this office has no separate exact application occurrence to replay.
 for(const auto& old:x.military.action_facts)if(old.command=="office_accept"&&old.phase=="completed"&&old.success){const auto* terms=std::get_if<OfficeTerms>(&old.terms);if(terms&&terms->office_id==office)fail("MilitaryOfficeLegacyCompletionHasNoExactApplication",old.id);}
 for(const auto& entry:x.actions.dynamic_action_facts.rows()){
  const auto& row=entry.second;const auto* terms=std::get_if<OfficeTerms>(&row.original_terms);
  if(row.command!="office_accept"||row.phase!="completed"||!row.success||!terms||terms->office_id!=office)continue;
  if(entry.first!=row.id||!row.terminal_source_ref||!row.completed_at||row.terminal_source_ref->domain!="military"||row.terminal_source_ref->kind!="military_office_completion_applied")fail("MilitaryAuthorityExactAppointmentRequired",row.id);
  const auto* retained=x.actions.dynamic_action_facts.find(entry.first);if(retained!=&row)fail("MilitaryAuthorityAppointmentOwnerMismatch",row.id);
  exact_event(*row.terminal_source_ref,*row.completed_at,{0,retained,nullptr,nullptr,nullptr},row.id);
 }
 for(const auto& row:x.military.resignation_facts)if(row.office_id==office){
  if(!row.exact_resignation)fail("MilitaryOfficeResignationSourceRequired",row.id);const auto& saved=*row.exact_resignation;
  if(saved.actual_resignation_source.domain!="military"||saved.actual_resignation_source.kind!="office_resignation"||saved.actual_resignation_source.id!=row.id)fail("MilitaryOfficeResignationSourceRequired",row.id);
  exact_event(saved.actual_resignation_source,saved.resigned_at,{0,nullptr,&row,nullptr,nullptr},row.id);
 }
 for(const auto& row:x.military.occupation_facts){
  if(row.garrison_id!=definition->garrison_id)continue;
  if(!row.disabled_office_ids)fail("MilitaryOccupationDisabledOfficesRequired",row.id);
  if(std::find(row.disabled_office_ids->begin(),row.disabled_office_ids->end(),office)==row.disabled_office_ids->end())continue;
  if(x.office.access.occupation_source_schema!=2||!row.exact_time)fail("MilitaryOfficeOccupationExactMomentRequired",row.id);
  exact_event(SourceRefBuildView{"military","military_occupation",row.id},row.exact_time->actual_occupation_at,{0,nullptr,nullptr,&row,nullptr},row.id);
 }
 for(const auto& row:x.military.integration_facts){
  const auto position=take(read_actual_common_fact_position(x.common,SourceRefBuildView{"military","military_integration",row.id}));
  // The real integration writer records Common.tick(), not the active exact M.
  // Its legal/nonfuture tick is checked independently. Only actual Common
  // ordinal orders it against this exact source cut; no M is manufactured.
  if(row.tick<0||row.tick>x.common.tick())fail("MilitaryIntegrationOfficeClockOrderMismatch",row.id);
  if(position<cut)events.push_back({position,nullptr,nullptr,nullptr,&row});
 }
 std::sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.position<b.position;});
 MilitaryOfficeAuthorityBeforeSource result;std::optional<std::size_t> previous;
 for(const auto& event:events){
  if(previous&&*previous==event.position)fail("MilitaryOfficeHistorySourceAmbiguous",office);previous=event.position;
  if(event.appointment){const auto& row=*event.appointment;const auto* actual=take(read_exact_office_completion_application(x.common,x.military,x.actions,x.elapsed,x.office,*row.terminal_source_ref,2));if(actual!=&row)fail("MilitaryAuthorityAppointmentSourceMismatch",row.id);result.appointment=actual;}
  else if(event.resignation){const auto& row=*event.resignation;take(read_military_office_resignation_source(x,row.exact_resignation->actual_resignation_source,1));result.appointment=nullptr;}
  else if(event.occupation){const auto& row=*event.occupation;auto memory=CheckpointMemoryCharge(0);const SourceRef source{"military","military_occupation",row.id};take(read_military_occupation_actual_moment(x.common,x.military,x.office.catalog,source,2,now,x.office.budget));result.appointment=nullptr;}
  else {const auto& row=*event.integration;take(validate_military_integration_appointment_source(x,row));
   for(const auto& ended:row.ended_appointments)if(ended.id==office){
    // This is the original preview current-office gate, replayed at this
    // actual event rather than against today's already-disabled Office.
    if(!result.appointment||result.appointment->id!=ended.appointment_id||result.appointment->actor_id!=ended.holder_actor_id)fail("MilitaryIntegrationOfficePredecessorMismatch",row.id);
    result.appointment=nullptr;
   }
  }
 }
 return result;
});}
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_before(const MilitaryAuthorityTimeContext& x,const StableId& office,const SourceRef& boundary,const WorldMoment& at){return read_military_office_authority_before_core(x,office,&boundary,nullptr,at);}
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_before(const MilitaryAuthoritySourceContext& x,const StableId& office,const SourceRef& boundary,const WorldMoment& at){auto bound=validate_military_authority_source_context(x);if(!bound.ok)return {false,{},std::move(bound.error)};return read_military_office_authority_before_core(x,office,&boundary,nullptr,at,x.window);}
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_at_actual_prefix(const MilitaryAuthoritySourceContext& x,const StableId& office,const CommonActualFactPrefix& prefix,const WorldMoment& actual_now,const MilitaryActualSourceWindow* window){
 auto bound=validate_military_authority_source_context(x);if(!bound.ok)return {false,{},std::move(bound.error)};
 if(x.window&&window&&x.window!=window)return {false,{},common_failure("MilitaryAuthoritySourceWindowMismatch",office).error};
 auto scoped=x;scoped.window=window?window:x.window;
 return read_military_office_authority_before_core(scoped,office,nullptr,&prefix,actual_now,scoped.window);
}
template<class Context>Status validate_military_authority_time_sources_core(const Context& x,Integer selected){return status([&]{
 if(selected!=0&&selected!=1)fail("UnsupportedMilitaryAuthorityTimeSchema");
 for(const auto& row:x.military.campaigns){if(selected==0){if(row.exact_acceptance)fail("UnexpectedMilitaryCampaignAcceptanceTime",row.id);}else{if(!row.exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",row.id);take(read_military_campaign_acceptance_source(x,row.exact_acceptance->actual_acceptance_source,1));}}
 for(const auto& row:x.military.resignation_facts){if(selected==0){if(row.exact_resignation)fail("UnexpectedMilitaryOfficeResignationTime",row.id);}else{if(!row.exact_resignation)fail("MilitaryOfficeResignationSourceRequired",row.id);take(read_military_office_resignation_source(x,row.exact_resignation->actual_resignation_source,1));}}
 for(const auto& head:x.common.state().facts.rows())if(head.second.domain=="military"&&head.second.kind=="campaign_accepted"){
  const auto* row=unique(x.military.campaigns,head.first);if(selected!=1||head.first!=head.second.id||!row||!row->exact_acceptance)fail("MilitaryCampaignAcceptanceOwnerRequired",head.first);
 }
 for(const auto& head:x.common.state().facts.rows())if(selected==1&&head.second.domain=="military"&&head.second.kind=="office_resignation"){
  const auto* row=unique(x.military.resignation_facts,head.first);if(head.first!=head.second.id||!row||!row->exact_resignation)fail("MilitaryOfficeResignationOwnerRequired",head.first);
 }
 if(selected==1){
  for(const auto& row:x.military.campaigns){
   const auto& time=*row.exact_acceptance;const auto authority=take(read_military_office_authority_before(x,row.office_id,time.actual_acceptance_source,time.accepted_at));
   if(!authority.appointment||!same_source(*authority.appointment->terminal_source_ref,time.office_appointment_source.source_ref))fail("MilitaryCampaignAcceptanceAuthorityLost",row.id);
   validate_original_authority_participants(x,row.actor_id,time.original_context,time.actual_acceptance_source,time.accepted_at);
  }
  for(const auto& row:x.military.resignation_facts){
   const auto& time=*row.exact_resignation;const auto authority=take(read_military_office_authority_before(x,row.office_id,time.actual_resignation_source,time.resigned_at));
   if(!authority.appointment||!same_source(*authority.appointment->terminal_source_ref,time.office_appointment_source.source_ref))fail("MilitaryOfficeResignationAuthorityLost",row.id);
   validate_original_authority_participants(x,row.actor_id,time.original_context,time.actual_resignation_source,time.resigned_at);
  }
 }
});}
Status validate_military_authority_time_sources(const MilitaryAuthorityTimeContext& x,Integer selected){return validate_military_authority_time_sources_core(x,selected);}
Status validate_military_authority_time_sources(const MilitaryAuthoritySourceContext& x,Integer selected){
 auto bound=validate_military_authority_source_context(x);if(!bound.ok)return bound;
 if(selected!=x.selected_authority_time)return common_failure("MilitaryAuthoritySourceSelectedMismatch");
 return validate_military_authority_time_sources_core(x,selected);
}

Status validate_military_authority_time_after_write(
    const CommonRead& common,const MilitaryState& state,const MilitaryAccess& access){return status([&]{
    take(validate_military_authority_time_write(common,state,access));
    if(access.military_authority_time_schema==0)return;
    take(validate_military_authority_time_sources(bound(common,state,access),1));
});}
namespace {
template<class Emit>Status visit_authority_pins(const MilitaryAuthorityTimeContext& x,Emit emit){return status([&]{
 take(validate_military_authority_time_sources(x,1));
 auto pin=[&](std::string_view kind,std::string_view id,std::string_view slot,const auto& source,SourcePinReason reason){
  take(emit(SourcePinBuildView{"military",kind,id,slot,source.domain,source.kind,source.id,reason}));
 };
 const auto now=take(x.office.clock.read_current_moment());
 auto garrison_pin=[&](std::string_view consumer,const StableId& id,std::string_view slot,const StableId& garrison,const StableId& organization,const SourceRef& boundary,const WorldMoment& at){
  const auto owner=take(read_military_garrison_owner_before_actual(x.common,x.military,x.office.catalog,x.office.access,garrison,SourceRefBuildView{boundary.domain,boundary.kind,boundary.id},at,now,x.office.access.occupation_source_schema,x.office.budget));
  if(owner.organization_id()!=organization)fail("MilitaryActionGarrisonAuthorityLost",id);
  if(owner.actual_occupation)pin(consumer,id,slot,SourceRefBuildView{"military","military_occupation",owner.actual_occupation->id},SourcePinReason::dependent_state);
 };
 for(const auto& row:x.military.campaigns){
  const auto& time=*row.exact_acceptance;
  pin("campaign",row.id,"acceptance_application",time.actual_acceptance_source,SourcePinReason::unique_outcome);
  pin("campaign_acceptance",row.id,"office_appointment",time.office_appointment_source.source_ref,SourcePinReason::dependent_state);
 }
 for(const auto& row:x.military.resignation_facts){
  const auto& time=*row.exact_resignation;
  pin("office_resignation",row.id,"actual_application",time.actual_resignation_source,SourcePinReason::unique_outcome);
  pin("office_resignation",row.id,"office_appointment",time.office_appointment_source.source_ref,SourcePinReason::dependent_state);
 }
 for(const auto& entry:x.actions.dynamic_action_facts.rows())if(entry.second.command=="army_form"){
  const auto& row=entry.second;const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);
  if(entry.first!=row.id||!terms||row.actor_id!=terms->actor_id)fail("ExactFormationAcceptanceIdentityMismatch",row.id);
  const auto* campaign=unique(x.military.campaigns,terms->order_id);
  if(!campaign||!campaign->exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",terms->order_id);
  const auto& accepted=*campaign->exact_acceptance;
  take(read_military_campaign_acceptance_source(x,accepted.actual_acceptance_source,1));
  auto memory=CheckpointMemoryCharge(0);const SourceRef start{"military","military_action",row.id};
  if(take(read_actual_common_fact_position(x.common,accepted.actual_acceptance_source))>=take(read_actual_common_fact_position(x.common,start))||take(compare_world_moments(accepted.accepted_at,row.started_at,x.office.budget))>0)fail("ExactFormationCampaignAcceptanceOrder",row.id);
  const auto authority=take(read_military_office_authority_before(x,terms->office_id,start,row.started_at));
  if(!authority.appointment||authority.appointment->actor_id!=row.actor_id)fail("ExactFormationOfficeAuthorityLost",row.id);
  pin("exact_formation_action",row.id,"campaign_acceptance",accepted.actual_acceptance_source,SourcePinReason::dependent_state);
  pin("exact_formation_action",row.id,"office_authority",*authority.appointment->terminal_source_ref,SourcePinReason::dependent_state);
  garrison_pin("exact_formation_action",row.id,"start_garrison_owner",terms->garrison_id,terms->organization_id,start,row.started_at);
  if(row.phase=="completed"&&row.success){
   if(!row.completed_at||!row.terminal_source_ref||row.terminal_source_ref->domain!="military"||row.terminal_source_ref->kind!="military_action_completion_applied")fail("ExactFormationCompletionApplicationRequired",row.id);
   const auto& terminal=*row.terminal_source_ref;const auto completed=take(read_military_office_authority_before(x,terms->office_id,terminal,*row.completed_at));
   if(!completed.appointment||completed.appointment->actor_id!=row.actor_id)fail("ExactFormationCompletionOfficeAuthorityLost",row.id);
   pin("exact_formation_action",row.id,"completion_office_authority",*completed.appointment->terminal_source_ref,SourcePinReason::dependent_state);
   garrison_pin("exact_formation_action",row.id,"completion_garrison_owner",terms->garrison_id,terms->organization_id,terminal,*row.completed_at);
  }
 }
 for(const auto& entry:x.actions.dynamic_logistics_facts.rows()){
  const auto& row=entry.second;const auto& terms=row.original_terms;
  if(row.command!="army_return")continue;
  if(entry.first!=row.id||row.actor_id!=terms.actor_id||!terms.authority)fail("ExactReturnAcceptanceIdentityMismatch",row.id);
  auto memory=CheckpointMemoryCharge(0);const SourceRef start{"military","military_action",row.id};
  if(terms.authority->kind=="office"){
   const auto authority=take(read_military_office_authority_before(x,terms.authority->id,start,row.started_at));
   if(!authority.appointment||authority.appointment->actor_id!=row.actor_id)fail("ExactReturnOfficeAuthorityLost",row.id);
   pin("exact_return_action",row.id,"office_authority",*authority.appointment->terminal_source_ref,SourcePinReason::dependent_state);
  }
  garrison_pin("exact_return_action",row.id,"start_garrison_owner",terms.garrison_id,terms.organization_id,start,row.started_at);
  if(row.phase=="completed"&&row.success){
   if(!row.completed_at||!row.terminal_source_ref||row.terminal_source_ref->domain!="military"||row.terminal_source_ref->kind!="military_logistics_completion_applied")fail("ExactReturnCompletionApplicationRequired",row.id);
   garrison_pin("exact_return_action",row.id,"completion_garrison_owner",terms.garrison_id,terms.organization_id,*row.terminal_source_ref,*row.completed_at);
  }
 }
});}
}
Status collect_military_authority_time_pins(const MilitaryAuthorityTimeContext& x,SourcePinSink& sink){
 return visit_authority_pins(x,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});
}
Status validate_military_authority_time_pin(const MilitaryAuthorityTimeContext& x,const SourcePin& input){return status([&]{
 bool matched=false;take(visit_authority_pins(x,[&](const SourcePinBuildView& pin){
  if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)matched=true;
  return common_success();
 }));if(!matched)fail("MilitaryAuthorityTimePinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> military_authority_time_consumer_slots(){return {
 {"military","campaign","acceptance_application"},
 {"military","campaign_acceptance","office_appointment"},
 {"military","office_resignation","actual_application"},
 {"military","office_resignation","office_appointment"},
 {"military","exact_formation_action","campaign_acceptance"},
 {"military","exact_formation_action","office_authority"},
 {"military","exact_return_action","office_authority"},
 {"military","exact_formation_action","start_garrison_owner"},
 {"military","exact_formation_action","completion_office_authority"},
 {"military","exact_formation_action","completion_garrison_owner"},
 {"military","exact_return_action","start_garrison_owner"},
 {"military","exact_return_action","completion_garrison_owner"}
};}
Outcome<bool> military_authority_time_source_may_release(const MilitaryAuthorityTimeContext& x,const SourceRef& source,Integer schema){return guard<bool>([&]{
 if(source.kind=="campaign_accepted")take(read_military_campaign_acceptance_source(x,source,schema));
 else if(source.kind=="office_resignation")take(read_military_office_resignation_source(x,source,schema));
 else fail("MilitaryAuthorityActualSourceRequired",source.id);
 // Both real applications remain owned by their original permanent result.
 return false;
});}
Status validate_military_authority_header_source(const FactRecord& head,const MilitaryState& military,const MilitaryActionTimeState& actions,Integer authority_schema,Integer action_time_schema){return status([&]{
 if(head.id.empty()||head.domain!="military")fail("MilitaryAuthorityHeaderMismatch",head.id);
 if(head.kind=="campaign_accepted"){
  if(authority_schema!=1)fail("MilitaryAuthorityTimeSelectedSchemaRequired",head.id);
  const auto* row=unique(military.campaigns,head.id);if(!row||!row->exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",head.id);
  const auto& source=row->exact_acceptance->actual_acceptance_source;
  if(source.domain!=head.domain||source.kind!=head.kind||source.id!=head.id)fail("MilitaryAuthorityHeaderMismatch",head.id);
 }else if(head.kind=="military_office_completion_applied"){
  if(action_time_schema!=1)fail("MilitaryActionTimeSelectedSchemaRequired",head.id);
  const DynamicMilitaryActionFact2* found=nullptr;
  for(const auto& item:actions.dynamic_action_facts.rows()){
   const auto& row=item.second;if(!row.terminal_source_ref||row.terminal_source_ref->id!=head.id)continue;
   if(found||item.first!=row.id||row.command!="office_accept"||row.rule_version!=2||row.phase!="completed"||!row.completed_at||!row.result||!std::holds_alternative<OfficeTerms>(row.original_terms))fail("MilitaryAuthorityHeaderMismatch",head.id);
   const auto& source=*row.terminal_source_ref;const auto& terms=std::get<OfficeTerms>(row.original_terms);
   if(source.domain!=head.domain||source.kind!=head.kind||head.id==row.id||row.actor_id!=terms.actor_id||row.result->ok||!row.result->army_id.empty()||!row.result->order_id.empty()||(row.success?row.result->office_id!=terms.office_id:!row.result->office_id.empty()))fail("MilitaryAuthorityHeaderMismatch",head.id);found=&row;
  }
  if(!found)fail("ExactOfficeCompletionApplicationRequired",head.id);
  for(const auto& row:actions.dynamic_logistics_facts.rows())if(row.second.terminal_source_ref&&row.second.terminal_source_ref->id==head.id)fail("DuplicateMilitaryCompletionApplication",head.id);
 }else if(head.kind=="military_formation_completion_checked"){
  if(action_time_schema!=1)fail("MilitaryActionTimeSelectedSchemaRequired",head.id);const DynamicMilitaryActionFact2* found=nullptr;
  for(const auto& item:actions.dynamic_action_facts.rows()){
   const auto& row=item.second;if(!row.terminal_source_ref||row.terminal_source_ref->id!=head.id)continue;const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);
   if(found||item.first!=row.id||row.command!="army_form"||row.rule_version!=2||row.phase!="completed"||row.success||!row.completed_at||!row.result||!terms||row.actor_id!=terms->actor_id||row.result->ok||!row.result->office_id.empty()||!row.result->army_id.empty()||!row.result->order_id.empty()||row.actual_resource_effect)fail("MilitaryFormationCompletionCheckHeaderMismatch",head.id);
   const auto& source=*row.terminal_source_ref;if(source.domain!=head.domain||source.kind!=head.kind||source.id==row.id)fail("MilitaryFormationCompletionCheckHeaderMismatch",head.id);found=&row;
  }
  if(!found)fail("MilitaryFormationCompletionCheckRequired",head.id);
  for(const auto& row:actions.dynamic_logistics_facts.rows())if(row.second.terminal_source_ref&&row.second.terminal_source_ref->id==head.id)fail("DuplicateMilitaryCompletionApplication",head.id);
 }else fail("UnsupportedMilitaryAuthorityHeaderKind",head.id);
});}
Status emit_military_authority_header_payload_fields(ProjectionWriter& sink,const FactRecord& head,const MilitaryState& military,const MilitaryActionTimeState& actions,Integer authority_schema,Integer action_time_schema){
 auto valid=validate_military_authority_header_source(head,military,actions,authority_schema,action_time_schema);if(!valid.ok)return valid;
 auto kind=sink.text("kind",head.kind);if(!kind.ok)return kind;return sink.text("domain",head.domain);
}
}
