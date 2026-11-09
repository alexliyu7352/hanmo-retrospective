#include "task_exact_life.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
Status consume_original_task_death_in_candidate(WorldBundle&w,const CharacterConditionWorldRead&original,const DeathRef&death){
 if(!w.tasks.exact_execution||w.tasks.exact_execution->version!=2||!w.world_timeline||!w.action_elapsed||!w.catalog||!w.catalog->tasks)
  return common_failure("TaskExactDeathProfileRequired",{},ErrorKind::unsupported_capability);
 auto body=original.sources();if(!body.ok)return {false,{},body.error};auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};
 if(&body.value.common!=&w.common||body.value.deaths!=&w.family_life||!body.value.life_sources||body.value.elapsed!=&*w.action_elapsed)
  return common_failure("TaskExactLifeCandidateMismatch",{},ErrorKind::internal);
 auto same=compare_world_moments(body.value.actual_now,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("TaskExactLifeMomentMismatch",{},ErrorKind::internal);
 TaskExactLifeSources life{w.common,w.family_life,*body.value.life_sources,body.value.actual_now};
 auto actual=read_verified_death4(w.common,life.deaths,life.causes,death,life.actual_now);if(!actual.ok)return {false,{},actual.error};
 if(actual.value.effective_at.kind!=ActorFoundationAt::Kind::exact_moment)return common_failure("TaskExactDeathMomentRequired",death.id);
 same=compare_world_moments(actual.value.effective_at.exact_moment,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("TaskExactDeathCurrentMomentMismatch",death.id);
 // The start table is not mutated by stopping or releasing an action. No
 // copied candidate list or current actor-action guess supplies ownership.
 for(const auto&row:w.tasks.exact_execution->starts.rows()){
  const auto&start=row.second;
  if(start.actor_id!=actual.value.actor_id&&start.accepted_action.context.provider_actor_id!=actual.value.actor_id)continue;
  const TaskExactTerminalSource*ended=nullptr;
  for(const auto&entry:w.tasks.exact_execution->terminals.rows())if(entry.second.action_id==start.action_id){if(ended)return common_failure("DuplicateTaskExactTerminal",start.action_id);ended=&entry.second;}
  if(ended)continue; // An already completed/stopped Action is never re-opened.
  TaskExactWrite write{*w.action_elapsed,w.world_timeline->scale_ref,now.value,w.catalog->identity};write.life=&life;
  auto stopped=stop_exact_task_for_death_in_candidate(w.common,w.tasks,*w.catalog->tasks,start.action_id,death,write,life);if(!stopped.ok)return {false,{},stopped.error};
  auto checked=guard_task_exact_release(w.common,w.tasks,*w.action_elapsed,start.action_id,&life);if(!checked.ok)return checked;
  checked=release_action(w,start.action_id);if(!checked.ok)return checked;
  if(w.common.action(start.action_id)||w.tasks.actions.find(start.action_id))return common_failure("TaskExactDeathActionNotReleased",start.action_id);
 }
 return common_success();
}
}
