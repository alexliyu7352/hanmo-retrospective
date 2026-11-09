#include "office_exact_detail.hpp"
#include "military_authority_time.hpp"
#include <set>
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/actor_foundation/character_availability_provenance.hpp"
namespace hanmo::resident {using namespace office_exact_detail;namespace office_exact_detail {
namespace {
template<class Context>void validate_original_identity(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const Context& x,const DynamicMilitaryActionFact2& f,CommonActualFactPrefixSink* retained_prefix=nullptr,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr){
 bound(c,military,state,x);header(c,f.id);const auto* t=std::get_if<OfficeTerms>(&f.original_terms);
 if(!t||f.command!="office_accept"||f.id.empty()||f.actor_id!=t->actor_id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=f.id||t->provider_actor_id.empty()||t->provider_actor_id==t->actor_id||!f.original_context||f.original_context->input_error||f.original_context->provider_actor_id!=t->provider_actor_id||f.original_context->place_id!=t->place_id)fail("ExactOfficeAcceptanceIdentityMismatch",f.id);
 const OfficeDefinition* definition=nullptr;for(const auto& row:x.catalog.offices)if(row.id==t->office_id)definition=&row;
 const auto* role=definition&&x.access.interaction_catalog&&x.access.interaction_catalog->common?x.access.interaction_catalog->common->roles.find(definition->appointing_role_id):nullptr;
 if(!definition||!role||definition->organization_id!=t->organization_id||f.original_context->role_id!=definition->appointing_role_id||role->place_id!=t->place_id||role->organization_id!=t->organization_id||!c.actor(t->actor_id)||!c.actor(t->provider_actor_id))fail("ExactOfficeOriginalTermsMismatch",f.id);
 for(const auto& legacy:military.action_facts)if(legacy.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& legacy:military.actions)if(legacy.action_id==f.id)fail("DuplicateLegacyExactMilitaryExecution",f.id);
 validate_original_role_source(c,x,f,false,retained_prefix,start_role);

}
}
void validate_original_office_identity(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactOfficeSourceReadContext& x,const DynamicMilitaryActionFact2& f){validate_original_identity(c,m,s,x,f);}
template<class Context>void validate_original_role_source_core(const CommonStore& c,const Context& x,const DynamicMilitaryActionFact2& f,bool completion,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* role_output){
 if(completion&&!f.success){
  if(role_output)role_output->reset();
  return;
 }
 if(!f.original_context)fail("ExactOfficeAcceptanceIdentityMismatch",f.id);
 const FactRecord* actual=nullptr;
 if(completion){
  if(!f.terminal_source_ref||f.terminal_source_ref->domain!="military"||f.terminal_source_ref->kind!="military_office_completion_applied")fail("ExactOfficeCompletionApplicationRequired",f.id);
  actual=c.state().facts.find(f.terminal_source_ref->id);
  if(!actual||actual->id!=f.terminal_source_ref->id||actual->domain!=f.terminal_source_ref->domain||actual->kind!=f.terminal_source_ref->kind)fail("ExactOfficeCompletionApplicationRequired",f.id);
 }else{header(c,f.id);actual=c.state().facts.find(f.id);}
 auto role=take(validate_military_original_role_before_source(
  c,x,*actual,*f.original_context,"office_accept",retained_prefix));
 if(role_output)*role_output=role;
}
void validate_original_role_source(const CommonStore& c,const ExactOfficeReadContext& x,const DynamicMilitaryActionFact2& f,bool completion,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* role_output){return validate_original_role_source_core(c,x,f,completion,retained_prefix,role_output);}
void validate_original_role_source(const CommonStore& c,const ExactOfficeSourceReadContext& x,const DynamicMilitaryActionFact2& f,bool completion,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* role_output){return validate_original_role_source_core(c,x,f,completion,retained_prefix,role_output);}
void validate_completion_header(const CommonStore& c,const MilitaryActionTimeState& state,const DynamicMilitaryActionFact2& f){
 if(f.phase!="completed"||!f.completed_at||!f.result||!f.terminal_source_ref||f.terminal_source_ref->domain!="military"||f.terminal_source_ref->kind!="military_office_completion_applied"||f.terminal_source_ref->id==f.id)fail("ExactOfficeCompletionApplicationRequired",f.id);
 const auto begin=take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_action",f.id}));
 const auto end=take(read_actual_common_fact_position(c,*f.terminal_source_ref));if(begin>=end)fail("ExactOfficeCompletionSourceOrder",f.id);
 for(const auto& row:state.dynamic_action_facts.rows())if(row.first!=f.id&&row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,*f.terminal_source_ref))fail("DuplicateMilitaryCompletionApplication",f.terminal_source_ref->id);
 for(const auto& row:state.dynamic_logistics_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,*f.terminal_source_ref))fail("DuplicateMilitaryCompletionApplication",f.terminal_source_ref->id);
}
template<class Context>const DynamicMilitaryActionFact2& source_local_office_core(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const Context& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role){
 if(schema!=2||ref.domain!="military"||ref.kind!="military_office_completion_applied")fail("ExactOfficeCompletionApplicationRequired",ref.id);
 const DynamicMilitaryActionFact2* found=nullptr;for(const auto& row:state.dynamic_action_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref)){if(found||row.first!=row.second.id)fail("DuplicateMilitaryCompletionApplication",ref.id);found=&row.second;}
 if(!found)fail("ExactOfficeCompletionApplicationRequired",ref.id);const auto& f=*found;validate_original_identity(c,military,state,x,f,retained_prefix,start_role);validate_completion_header(c,state,f);const auto& terms=std::get<OfficeTerms>(f.original_terms);
 if(!same_moment(*f.completed_at,due(f,x.budget),x.budget)||f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty()||f.result->office_id!=(f.success?terms.office_id:StableId{}))fail("ExactOfficeCompletedResultMismatch",f.id);
 validate_original_role_source(c,x,f,true,retained_prefix,successful_completion_role);
 const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry||entry->action_ref!=f.id||entry->execution_source_ref!=f.id||entry->owner_cost_state_ref!=f.id||entry->duration_definition_ref!="office_accept_original_1_half_day_v2"||compare(entry->required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget))fail("ExactOfficeElapsedBindingMismatch",f.id);
 const auto service=take(f.success?read_fixed_service_completion(elapsed,f.elapsed_source_ref,take(x.clock.read_current_moment()),x.scale,x.budget):read_fixed_service_completed_recheck_interruption(elapsed,f.elapsed_source_ref,take(x.clock.read_current_moment()),x.scale,x.budget));if(!service.actual_service.complete||!same_moment(service.started_at,f.started_at,x.budget)||!same_moment(service.completed_at,*f.completed_at,x.budget))fail("ExactOfficeFullActualServiceRequired",f.id);return f;
}
const DynamicMilitaryActionFact2& source_local_office(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role){return source_local_office_core(c,military,state,elapsed,x,ref,schema,retained_prefix,start_role,successful_completion_role);}
const DynamicMilitaryActionFact2& source_local_office(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeSourceReadContext& x,const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role){if(&x.elapsed!=&elapsed)fail("CrossCandidateExactOfficeSourceRoots");return source_local_office_core(c,military,state,elapsed,x,ref,schema,retained_prefix,start_role,successful_completion_role);}
void validate_original_interaction_availability(
    const CommonStore& c,const MilitaryState& military,const ExactOfficeReadContext& x,
    const DynamicMilitaryActionFact2& f,bool completion,
    CharacterAvailabilityProvenanceSink* retained_body){
    if(completion&&!f.success)return;
    if(&x.body.common!=&c||(x.body.military&&x.body.military!=&military))
        fail("ExactOfficeBodySourceRootMismatch",f.id);
    const auto prove=[&](const SourceRef& boundary,const WorldMoment& at){
        for(const auto* actor:participant_refs(f)){
            const auto available=take(retained_body
                ?visit_character_availability_before_fact_provenance_v2(
                    x.body,*actor,boundary,at,x.clock,*retained_body)
                :read_character_availability_before_fact_provenance_v2(
                    x.body,*actor,boundary,at,x.clock));
            if(!available.present||!available.alive||!available.available)
                fail("ExactOfficeOriginalParticipantAvailabilityMismatch",*actor);
        }
    };
    if(completion){
        if(!f.completed_at||!f.terminal_source_ref)
            fail("ExactOfficeCompletionApplicationRequired",f.id);
        prove(*f.terminal_source_ref,*f.completed_at);
        return;
    }


    auto input_memory=CheckpointMemoryCharge(0);
    const auto source=start_ref(f.id);
    prove(source,f.started_at);
}
Status validate_fact_history(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactOfficeReadContext& x,const DynamicMilitaryActionFact2& f,const ActionElapsedState& entry,bool pending,CharacterAvailabilityProvenanceSink* retained_body){return status([&]{
 validate_original_identity(c,military,state,x,f);const auto* t=std::get_if<OfficeTerms>(&f.original_terms);
 take(x.original.validate_office_acceptance(f));const auto now=take(x.clock.read_current_moment());
 if(entry.action_ref!=f.id||entry.execution_source_ref!=f.id||entry.duration_definition_ref!="office_accept_original_1_half_day_v2"||entry.owner_cost_state_ref!=f.id||entry.owner_work_remainder||!same_moment(entry.started_at,f.started_at,x.budget)||compare(entry.required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget)||take(compare_world_moments(entry.last_settled_at,f.started_at,x.budget))<0||take(compare_world_moments(entry.last_settled_at,now,x.budget))>0)fail("ExactOfficeElapsedBindingMismatch",f.id);
 const auto credited=take(duration_to_scale_units(take(elapsed_world_duration(f.started_at,entry.last_settled_at,x.budget)),x.scale.units_per_half_day,x.budget));
 if(compare(credited.value,entry.credited_units.value,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)>0)fail("ExactOfficeActualServiceMismatch",f.id);
 if(take(compare_world_moments(f.started_at,entry.last_settled_at,x.budget))<0)take(x.original.validate_office_actual_service(f,{f.started_at,entry.last_settled_at}));
 if(f.phase=="started"){
  if(f.completed_at||f.result||f.success||f.terminal_source_ref)fail("ExactOfficePrematureResult",f.id);
  const auto order=take(compare_world_moments(entry.last_settled_at,due(f,x.budget),x.budget));if(order>0||(!pending&&order==0))fail("ExactOfficeUnconsumedCompletion",f.id);
 }else if(f.phase=="completed"){
  // The selected /2 result owns its actual Common completion application.
  // Legacy /1 keeps its separate interpretation; a missing past head is not repaired.
  validate_completion_header(c,state,f);
  if(!f.completed_at||!f.result||!same_moment(*f.completed_at,due(f,x.budget),x.budget)||!same_moment(*f.completed_at,entry.last_settled_at,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)||f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty()||f.result->office_id!=(f.success?t->office_id:StableId{}))fail("ExactOfficeCompletedResultMismatch",f.id);
  validate_original_role_source(c,x,f,true);
  take(x.original.validate_office_completed_result(f));
 }else fail("InvalidExactOfficePhase",f.id);
 validate_original_interaction_availability(c,military,x,f,false,retained_body);
 if(f.phase=="completed")validate_original_interaction_availability(c,military,x,f,true,retained_body);
});}
}
Status validate_exact_office_sources(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,bool pending){return status([&]{
 NativeOfficeActionTimeRead reader(c,military,state,elapsed,x);const auto reg=exact_office_action_registration();
 for(const auto& row:state.dynamic_action_facts.rows())if(row.second.command=="office_accept"){
  const auto& f=row.second;if(row.first!=f.id)fail("ExactOfficeSourceKeyMismatch",row.first);const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry)fail("ExactOfficeElapsedRequired",f.id);take(validate_fact_history(c,military,state,x,f,*entry,pending));
  if(pending&&f.phase=="started")take(read_fixed_service_intervals_at_completion_due(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));else take(read_fixed_service_intervals(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));
  const auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& item:elapsed.fixed_service_transitions)if(item.action_ref==f.id)transitions.push_back(item);take(reader.validate_fixed_action_sources(reg,proof,*entry,transitions));
  if(f.phase=="started"){const auto* a=c.action(f.id);if(!a||!std::holds_alternative<ElapsedRunning>(entry->timing)||a->remaining_ticks!=1)fail("ExactOfficeActiveEnvelopeMismatch",f.id);for(const auto* participant:participant_refs(f)){const auto& id=*participant;const auto* actor=c.actor(id);if(!actor||actor->action_id!=f.id)fail("ExactOfficeOccupationMissing",id);}}
  else if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(entry->timing))fail("ExactOfficeTerminalNotReleased",f.id);
 }
 for(const auto& item:c.state().facts.rows())if(item.second.domain=="military"&&item.second.kind=="military_office_completion_applied"){
  if(item.first!=item.second.id)fail("ExactOfficeCompletionSourceKeyMismatch",item.first);
  const SourceRef* owned=nullptr;for(const auto& row:state.dynamic_action_facts.rows())if(row.second.terminal_source_ref&&row.second.terminal_source_ref->id==item.first){if(owned)fail("DuplicateMilitaryCompletionApplication",item.first);owned=&*row.second.terminal_source_ref;}
  if(!owned)fail("ExactOfficeCompletionApplicationRequired",item.first);source_local_office(c,military,state,elapsed,x,*owned,2);
 }
 // The Common kind is shared by original Army/Logistics. Only exact Office
 // rows are consumed here; the complete annex validates their other owners.
 for(const auto& row:c.state().actions.rows())if(row.second.kind=="office_accept"){
  bool legacy=false;for(const auto& old:military.action_facts)if(old.id==row.first)legacy=true;
  if(!legacy&&!state.dynamic_action_facts.find(row.first))fail("MissingExactOfficeSource",row.first);
 }
});}
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_office_result(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,const SourceRef& ref,Integer schema,CharacterAvailabilityProvenanceSink* retained_body){return guard<const DynamicMilitaryActionFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_action_completed")fail("ExactOfficeCompletedSourceRequired",ref.id);const auto& f=fact(s,ref.id);const auto* e=find_fixed_elapsed(elapsed,f.id);if(!e||f.phase!="completed")fail("ExactOfficeCompletedSourceRequired",ref.id);
 take(validate_fact_history(c,m,s,x,f,*e,false,retained_body));take(read_fixed_service_intervals(elapsed,f.id,f.started_at,*f.completed_at,take(x.clock.read_current_moment()),x.scale,x.budget));NativeOfficeActionTimeRead reader(c,m,s,elapsed,x);auto reg=exact_office_action_registration();auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& t:elapsed.fixed_service_transitions)if(t.action_ref==f.id)transitions.push_back(t);take(reader.validate_fixed_action_sources(reg,proof,*e,transitions));if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(e->timing))fail("ExactOfficeTerminalNotReleased",f.id);return &f;
});}
Outcome<const DynamicMilitaryActionFact2*> read_exact_office_completion_application(
    const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,
    const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,
    const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role){
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix> staged_start,staged_completion;
    auto verified=guard<const DynamicMilitaryActionFact2*>([&]{
        return &source_local_office(c,m,s,elapsed,x,ref,schema,retained_prefix,
            &staged_start,&staged_completion);
    });
    if(!verified.ok)return verified;
    if(start_role)*start_role=staged_start;
    if(successful_completion_role)*successful_completion_role=staged_completion;
    return verified;
}
Outcome<const DynamicMilitaryActionFact2*> read_exact_office_completion_application(
    const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,
    const ActionElapsedStore& elapsed,const ExactOfficeSourceReadContext& x,
    const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role){
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix> staged_start,staged_completion;
    auto verified=guard<const DynamicMilitaryActionFact2*>([&]{
        return &source_local_office(c,m,s,elapsed,x,ref,schema,retained_prefix,
            &staged_start,&staged_completion);
    });
    if(!verified.ok)return verified;
    if(start_role)*start_role=staged_start;
    if(successful_completion_role)*successful_completion_role=staged_completion;
    return verified;
}

Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_office_application(
    const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,
    const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,
    const SourceRef& ref,Integer schema,CommonActualFactPrefixSink* retained_prefix,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role,
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role,CharacterAvailabilityProvenanceSink* retained_body){
    std::optional<OriginalMilitaryInteractionRoleBeforePrefix> staged_start,staged_completion;
    auto verified=guard<const DynamicMilitaryActionFact2*>([&]{
        const auto& f=source_local_office(c,m,s,elapsed,x,ref,schema,retained_prefix,
            &staged_start,&staged_completion);

        auto input_memory=CheckpointMemoryCharge(0);const auto completed=completed_ref(f.id);
        return take(read_verified_exact_office_result(c,m,s,elapsed,x,completed,2,retained_body));
    });
    if(!verified.ok)return verified;
    if(start_role)*start_role=staged_start;
    if(successful_completion_role)*successful_completion_role=staged_completion;
    return verified;
}

namespace {
template<class Emit>Status visit_office_completion_pins(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,Emit emit){return status([&]{
 for(const auto& row:state.dynamic_action_facts.rows()){
  const auto& f=row.second;if(f.command!="office_accept"||f.phase!="completed")continue;
  if(row.first!=f.id||!f.terminal_source_ref)fail("ExactOfficeCompletionApplicationRequired",row.first);
  source_local_office(c,m,state,elapsed,x,*f.terminal_source_ref,2);const auto& ref=*f.terminal_source_ref;
  take(emit(SourcePinBuildView{"military","exact_office_action",f.id,"completion_application",ref.domain,ref.kind,ref.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","office_completion_application",ref.id,"original_action","military","military_action",f.id,SourcePinReason::unique_outcome}));
  for(const auto& office:m.offices)if(office.appointment_id==f.id){
   const auto& terms=std::get<OfficeTerms>(f.original_terms);if(!f.success||office.id!=terms.office_id||office.holder_actor_id!=f.actor_id)fail("ExactOfficeAppointmentResultMismatch",office.id);
   take(emit(SourcePinBuildView{"military","office",office.id,"appointment_completion",ref.domain,ref.kind,ref.id,office.enabled?SourcePinReason::active_authority:SourcePinReason::dependent_state}));
  }
 }
});}
}
Status collect_exact_office_completion_pins(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,SourcePinSink& output){
 return visit_office_completion_pins(c,m,state,elapsed,x,[&](const SourcePinBuildView& pin){return output.contains(pin)?common_success():output.append(pin);});
}
Status validate_exact_office_completion_pin(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,const SourcePin& input){return status([&]{
 bool matched=false;take(visit_office_completion_pins(c,m,state,elapsed,x,[&](const SourcePinBuildView& pin){
  if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)matched=true;
  return common_success();
 }));if(!matched)fail("ExactOfficeCompletionPinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> exact_office_completion_consumer_slots(){return {
 {"military","exact_office_action","completion_application"},
 {"military","office_completion_application","original_action"},
 {"military","office","appointment_completion"}
};}
Outcome<bool> exact_office_completion_source_may_release(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,const SourceRef& source,Integer schema){return guard<bool>([&]{
 source_local_office(c,m,state,elapsed,x,source,schema);
 // This actual application belongs to a retained original action row. No
 // original erasure operation authorizes dropping it while retaining that row.
 return false;
});}

}
