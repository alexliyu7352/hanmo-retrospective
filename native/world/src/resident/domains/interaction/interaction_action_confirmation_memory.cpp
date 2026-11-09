#include "interaction_action_confirmation.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <utility>
namespace hanmo::resident {namespace {
InteractionActionConfirmationBuildView borrowed(const InteractionActionConfirmationSource&v){
 return {v.committed_result_id,v.session_id,v.session_generation,v.dialogue_id,
  v.action_node_id,v.confirmed_command,v.action_id,v.confirmation_at};
}
CheckpointCopyLease reserve(const InteractionActionConfirmationBuildView&v){

 return CheckpointCopyLease::reserve();
}
}
InteractionActionConfirmationSource::InteractionActionConfirmationSource(const InteractionActionConfirmationBuildView&v)
 :copied_fields(reserve(v)),committed_result_id(checkpoint_fresh_string_copy(v.committed_result_id)),
 session_id(checkpoint_fresh_string_copy(v.session_id)),session_generation(v.session_generation),
 dialogue_id(checkpoint_fresh_string_copy(v.dialogue_id)),action_node_id(checkpoint_fresh_string_copy(v.action_node_id)),
 confirmed_command(v.confirmed_command),action_id(checkpoint_fresh_string_copy(v.action_id)),confirmation_at(v.confirmation_at){}
InteractionActionConfirmationSource::InteractionActionConfirmationSource(const InteractionActionConfirmationSource&v)
 :InteractionActionConfirmationSource(borrowed(v)){}
InteractionActionConfirmationSource::InteractionActionConfirmationSource(InteractionActionConfirmationSource&&v)
 :copied_fields(v.copied_fields),committed_result_id(std::move(v.committed_result_id)),
 session_id(std::move(v.session_id)),session_generation(v.session_generation),
 dialogue_id(std::move(v.dialogue_id)),action_node_id(std::move(v.action_node_id)),
 confirmed_command(std::move(v.confirmed_command)),action_id(std::move(v.action_id)),confirmation_at(std::move(v.confirmation_at)){
 v.copied_fields={};
}
InteractionActionConfirmationSource& InteractionActionConfirmationSource::operator=(const InteractionActionConfirmationSource&v){
 if(this!=&v){InteractionActionConfirmationSource fresh(v);swap(fresh);}return *this;
}
InteractionActionConfirmationSource& InteractionActionConfirmationSource::operator=(InteractionActionConfirmationSource&&v){
 if(this!=&v){InteractionActionConfirmationSource fresh(std::move(v));swap(fresh);}return *this;
}
void InteractionActionConfirmationSource::swap(InteractionActionConfirmationSource&v){
 if(this==&v)return;auto left=copied_fields,right=v.copied_fields;
 auto both=CheckpointCopyLease::hold_both(left,right);copied_fields=both;v.copied_fields=both;
 // Exact M has genuinely fallible moves. Both original leaves remain owned
 // throughout a partially completed private swap, just as in original Life.
 using std::swap;swap(committed_result_id,v.committed_result_id);swap(session_id,v.session_id);
 swap(session_generation,v.session_generation);swap(dialogue_id,v.dialogue_id);
 swap(action_node_id,v.action_node_id);swap(confirmed_command,v.confirmed_command);
 swap(action_id,v.action_id);swap(confirmation_at,v.confirmation_at);
 copied_fields=std::move(right);v.copied_fields=std::move(left);
}
}
