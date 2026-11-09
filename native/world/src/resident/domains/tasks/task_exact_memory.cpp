#include "task_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <utility>
namespace hanmo::resident {namespace {
TaskExactStartBuildView borrowed(const TaskExactStartSource&v){
 return {v.id,v.action_id,v.action_kind,v.actor_id,v.participants,v.accepted_action,
  v.required_half_days,v.started_at,v.catalog_identity,v.terms_revision,v.actual_start_sources};
}
CheckpointCopyLease reserve(const TaskExactStartBuildView&v){
 return CheckpointCopyLease::reserve();
}
TaskExactTerminalBuildView borrowed(const TaskExactTerminalSource&v){
 std::optional<TaskExactParticipantDeathBuildView> death;
 if(v.participant_death)death.emplace(TaskExactParticipantDeathBuildView{v.participant_death->death_ref,v.participant_death->role});
 return {v.id,v.start_source_id,v.action_id,v.finished_at,v.effect,v.task_status_after,
  v.settlement_id_after,v.actual_result_sources,v.disposition,death};
}
CheckpointCopyLease reserve(const TaskExactTerminalBuildView&v){
 return CheckpointCopyLease::reserve();
}
std::optional<TaskExactParticipantDeath> copied(const std::optional<TaskExactParticipantDeathBuildView>&v){
 if(!v)return std::nullopt;
 return TaskExactParticipantDeath{{checkpoint_fresh_string_copy(v->death_ref.domain),
  checkpoint_fresh_string_copy(v->death_ref.kind),checkpoint_fresh_string_copy(v->death_ref.id)},v->role};
}
}
TaskExactStartSource::TaskExactStartSource(const TaskExactStartBuildView&v)
 :copied_fields(reserve(v)),id(checkpoint_fresh_string_copy(v.id)),action_id(checkpoint_fresh_string_copy(v.action_id)),
 action_kind(checkpoint_fresh_string_copy(v.action_kind)),actor_id(checkpoint_fresh_string_copy(v.actor_id)),
 participants(v.participants),accepted_action(v.accepted_action),required_half_days(v.required_half_days),
 started_at(v.started_at),catalog_identity(v.catalog_identity),terms_revision(v.terms_revision),actual_start_sources(v.actual_start_sources){}
TaskExactStartSource::TaskExactStartSource(const TaskExactStartSource&v):TaskExactStartSource(borrowed(v)){}
// A decoded source may have only its enclosing decode lease. This original
// fallible move makes a fresh charged target before that source can disappear.
TaskExactStartSource::TaskExactStartSource(TaskExactStartSource&&v):TaskExactStartSource(borrowed(v)){}
TaskExactStartSource& TaskExactStartSource::operator=(const TaskExactStartSource&v){if(this!=&v){TaskExactStartSource fresh(v);swap(fresh);}return *this;}
TaskExactStartSource& TaskExactStartSource::operator=(TaskExactStartSource&&v){if(this!=&v){TaskExactStartSource fresh(std::move(v));swap(fresh);}return *this;}
void TaskExactStartSource::swap(TaskExactStartSource&v){
 if(this==&v)return;auto left=copied_fields,right=v.copied_fields;
 auto both=CheckpointCopyLease::hold_both(left,right);copied_fields=both;v.copied_fields=both;
 using std::swap;swap(id,v.id);swap(action_id,v.action_id);swap(action_kind,v.action_kind);swap(actor_id,v.actor_id);
 swap(participants,v.participants);swap(accepted_action,v.accepted_action);swap(required_half_days,v.required_half_days);
 swap(started_at,v.started_at);swap(catalog_identity,v.catalog_identity);swap(terms_revision,v.terms_revision);
 swap(actual_start_sources,v.actual_start_sources);copied_fields=std::move(right);v.copied_fields=std::move(left);
}
TaskExactTerminalSource::TaskExactTerminalSource(const TaskExactTerminalBuildView&v)
 :copied_fields(reserve(v)),id(checkpoint_fresh_string_copy(v.id)),
 start_source_id(checkpoint_fresh_string_copy(v.start_source_id)),action_id(checkpoint_fresh_string_copy(v.action_id)),
 finished_at(v.finished_at),effect(v.effect),task_status_after(checkpoint_fresh_string_copy(v.task_status_after)),
 settlement_id_after(checkpoint_fresh_string_copy(v.settlement_id_after)),actual_result_sources(v.actual_result_sources),
 disposition(v.disposition),participant_death(copied(v.participant_death)){}
TaskExactTerminalSource::TaskExactTerminalSource(const TaskExactTerminalSource&v):TaskExactTerminalSource(borrowed(v)){}
// Like a start, an independently returned terminal must not borrow the former
// decoded root's zero-own-credit lifetime. Reserve its real fresh target first.
TaskExactTerminalSource::TaskExactTerminalSource(TaskExactTerminalSource&&v):TaskExactTerminalSource(borrowed(v)){}
TaskExactTerminalSource& TaskExactTerminalSource::operator=(const TaskExactTerminalSource&v){if(this!=&v){TaskExactTerminalSource fresh(v);swap(fresh);}return *this;}
TaskExactTerminalSource& TaskExactTerminalSource::operator=(TaskExactTerminalSource&&v){if(this!=&v){TaskExactTerminalSource fresh(std::move(v));swap(fresh);}return *this;}
void TaskExactTerminalSource::swap(TaskExactTerminalSource&v){
 if(this==&v)return;auto left=copied_fields,right=v.copied_fields;
 auto both=CheckpointCopyLease::hold_both(left,right);copied_fields=both;v.copied_fields=both;
 // Exact Moment moves may fail. Keep the actual old leaves until the full
 // private swap succeeds; failure discards the unpublished whole candidate.
 using std::swap;swap(id,v.id);swap(start_source_id,v.start_source_id);swap(action_id,v.action_id);
 swap(finished_at,v.finished_at);swap(effect,v.effect);swap(task_status_after,v.task_status_after);
 swap(settlement_id_after,v.settlement_id_after);swap(actual_result_sources,v.actual_result_sources);
 swap(disposition,v.disposition);swap(participant_death,v.participant_death);
 copied_fields=std::move(right);v.copied_fields=std::move(left);
}
}
