#include "growth_recovery_terminal_v1.hpp"
#include "growth_model.hpp"
#include "growth_action_time_read.hpp"
#include "growth_recovery_model.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {
namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& action={}) {
 throw Failure{common_failure(code,action).error};
}
template<class T>T take(Outcome<T> value) {
 if(!value.ok)throw Failure{std::move(value.error)};
 return std::move(value.value);
}
void selected(const CharacterConditionSources& body) {
 Integer source=0,condition=0;
 for(const auto& item:body.catalog.identity.module_versions) {
  if(item.first=="character-injury-sources") {
   if(item.second!="1"||++source!=1)fail("CharacterInjurySourceVersion1Required");
  }
  if(item.first=="character_condition_extensions") {
   if(item.second!="8"||++condition!=1)fail("ConditionInjurySourcePhysical8Required");
  }
 }
 if(source!=1||condition!=1)fail("CharacterInjurySourceSelectionRequired");
}
}
Outcome<GrowthTerminalRecoverySourceReadV1> read_growth_terminal_recovery_source_v1(
 const CharacterConditionSources& body,const StableId& action) {
 try {
  selected(body);
  if(!body.growth||!body.growth_catalog||!body.shared||!body.elapsed||!body.scale||
     !body.action_registrations||!body.injury_recovery_catalog||
     !body.growth_catalog->complete_capability_application_chain)
   fail("TerminalRecoveryOriginalWorldSourcesRequired",action);
  const auto* activity=body.growth->life_activities.find(action);
  if(!activity||activity->view.action_id!=action||activity->view.fact_id!=action||
     activity->view.kind!=LifeActivityKind::service||activity->view.activity!="rest"||
     activity->view.phase!=LifeActivityPhase::completed||!activity->view.success||
     !activity->view.code.empty())
   fail("TerminalRecoverySuccessfulOriginalServiceRequired",action);
  const auto* method=body.injury_recovery_catalog->service_roles.find(activity->context.role_id);
  if(!method)fail("TerminalRecoveryOriginalMethodRequired",action);
  const Integer points=growth_recovery_service_points(*method);
  if(points!=1&&points!=2)fail("TerminalRecoveryOriginalMethodRequired",action);
  const ActionTimeRegistration* registration=nullptr;
  for(const auto& row:*body.action_registrations)
   if(row.owner_domain=="growth"&&row.action_kind=="service") {
    if(registration)fail("DuplicateTerminalRecoveryRegistration",action);
    registration=&row;
   }
  if(!registration)fail("TerminalRecoveryOriginalRegistrationRequired",action);
  GrowthActionTimeRead time(body.common,*body.growth,*body.growth_catalog,*body.shared,
     *body.elapsed,*body.scale,body.catalog.identity,body.actual_now,{},
     body.interaction,body.interaction_catalog);
  auto owner=take(time.read_action_time_owner(*registration,action));
  if(owner.phase!=ActionTimeOwnerPhase::terminal||!owner.ended_source||
     owner.actor_id!=activity->view.actor_id||
     owner.participant_actor_ids!=activity->view.participant_actor_ids)
   fail("TerminalRecoveryOriginalOwnerMismatch",action);
  const auto* elapsed=find_fixed_elapsed(*body.elapsed,action);
  if(!elapsed||!std::holds_alternative<ElapsedTerminal>(elapsed->timing))
   fail("TerminalRecoveryActualServiceRequired",action);
  take(time.validate_retained_fixed_action_sources(*registration,owner,*elapsed));
  // The existing completion view owns one entry_ref string in addition to
  // this actual argument. Reserve both first copies before either is made.

  auto completion_memory=CheckpointMemoryCharge(0);
  const ActionElapsedEntryRef entry_ref{action};
  auto supplied=take(read_fixed_service_completion(*body.elapsed,entry_ref,body.actual_now,*body.scale));
  const LifeFixedServiceBoundary* terminal=nullptr;
  for(const auto& row:body.growth->fixed_service_boundaries.rows())
   if(row.second.action_ref==action&&row.second.kind==LifeFixedBoundaryKind::completed) {
    if(terminal)fail("DuplicateTerminalRecoveryCompletion",action);
    terminal=&row.second;
   }
  if(!terminal||take(compare_world_moments(terminal->occurred_at,supplied.completed_at))||
     take(compare_world_moments(terminal->occurred_at,take(time.read_action_time_source(*owner.ended_source)))))
   fail("TerminalRecoveryOriginalMomentMismatch",action);
  // The quantity comes from the actual validated active-service windows. Do
  // not credit a pause or reconstruct service from duration / wall-clock age.
  const auto credited=take(subtract_work_units(supplied.actual_service.credited_after,
                                               supplied.actual_service.credited_before));
  const auto duration=take(scale_units_to_duration(credited,body.scale->units_per_half_day));
  const Integer half_days=take(floor_fraction(duration.value));
  if(half_days<1||take(compare_fractions(credited.value,
      take(fixed_duration_work_units(half_days,body.scale->units_per_half_day)).value)))
   fail("TerminalRecoveryWholeProvidedHalfDaysRequired",action);
  return {true,{activity,terminal,points,half_days},{}};
 } catch(Failure& e) {return {false,{},std::move(e.error)};}
}
}
