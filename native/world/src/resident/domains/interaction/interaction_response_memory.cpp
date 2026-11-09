#include "interaction_response_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
#include <utility>
namespace hanmo::resident {namespace {
CheckpointMemoryCharge boundary_memory(const InteractionSessionBoundaryBuildView&value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge boundary_memory(const InteractionSessionBoundary&value){return CheckpointMemoryCharge(0);}
SourceRef fresh_source(const SourceRef&source){return {checkpoint_fresh_string_copy(source.domain),checkpoint_fresh_string_copy(source.kind),checkpoint_fresh_string_copy(source.id)};}
std::optional<SourceRef> fresh_optional_source(const SourceRef*source){return source?std::optional<SourceRef>{fresh_source(*source)}:std::nullopt;}

CheckpointMemoryCharge prepared_memory(const StableId&id,const StableId&session,const StableId&node,const StableId&choice,const StableId*next){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge response_memory(const InteractionResponseSource&value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge sealed_memory(const InteractionResponseSource&value,const SourceRef&actual){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge tail_memory(const SelectedResponseTail&value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge tail_memory(const StableId&session,const SourceRef&source,const InteractionResponseNext&next){return CheckpointMemoryCharge(0);}
}
InteractionSessionBoundary::InteractionSessionBoundary(const InteractionSessionBoundaryBuildView&value)
 :copy_memory(boundary_memory(value)),session_id(checkpoint_fresh_string_copy(value.session_id)),generation(value.generation),current_response_fact_ref(fresh_optional_source(value.current_response_fact_ref)),last_consumed_response_fact_ref(fresh_optional_source(value.last_consumed_response_fact_ref)){}
InteractionSessionBoundary::InteractionSessionBoundary(const InteractionSessionBoundary&other)
 :copy_memory(boundary_memory(other)),session_id(checkpoint_fresh_string_copy(other.session_id)),generation(other.generation),current_response_fact_ref(fresh_optional_source(other.current_response_fact_ref?&*other.current_response_fact_ref:nullptr)),last_consumed_response_fact_ref(fresh_optional_source(other.last_consumed_response_fact_ref?&*other.last_consumed_response_fact_ref:nullptr)){}
InteractionSessionBoundary::InteractionSessionBoundary(InteractionSessionBoundary&&other){swap(other);}
InteractionSessionBoundary&InteractionSessionBoundary::operator=(const InteractionSessionBoundary&other){if(this!=&other){InteractionSessionBoundary fresh(other);swap(fresh);}return *this;}
InteractionSessionBoundary&InteractionSessionBoundary::operator=(InteractionSessionBoundary&&other)noexcept{swap(other);return *this;}
void InteractionSessionBoundary::swap(InteractionSessionBoundary&other)noexcept{
 if(this==&other)return;static_assert(std::is_nothrow_swappable_v<std::optional<SourceRef>>);
 using std::swap;copy_memory.swap(other.copy_memory);session_id.swap(other.session_id);swap(generation,other.generation);current_response_fact_ref.swap(other.current_response_fact_ref);last_consumed_response_fact_ref.swap(other.last_consumed_response_fact_ref);
}
Status InteractionSessionBoundary::consume_current_response(){
 if(!current_response_fact_ref)return common_failure("InteractionResponseCurrentSourceRequired",session_id,ErrorKind::internal);

 last_consumed_response_fact_ref.reset();
 last_consumed_response_fact_ref.swap(current_response_fact_ref);return common_success();
}
Status InteractionResponseSource::retain_consumed_result_copy(const SourceRef&ref){
 if(tail_consumed_result_ref)return common_failure("InteractionResponseAlreadyConsumed",response_fact_id,ErrorKind::internal);
 InteractionResponseSource consumed(*this);(consumed.copy_memory).grow(0);
 consumed.tail_consumed_result_ref=SourceRef{checkpoint_fresh_string_copy(ref.domain),checkpoint_fresh_string_copy(ref.kind),checkpoint_fresh_string_copy(ref.id)};
 swap(consumed);return common_success();
}
InteractionResponseSource::InteractionResponseSource(const StableId&id,const StableId&session,Integer generation,const StableId&node,const StableId&choice,const StableId*next)
 :copy_memory(prepared_memory(id,session,node,choice,next)),response_fact_id(id),session_id(session),session_generation(generation),selected_node_id(node),selected_choice_id(choice),next_node_or_terminal(next?InteractionResponseNext{InteractionResponseNode{*next}}:InteractionResponseNext{InteractionResponseTerminal{}}){}
InteractionResponseSource::InteractionResponseSource(const InteractionResponseSource&prepared,const SourceRef&actual)
 :copy_memory(sealed_memory(prepared,actual)),response_fact_id(prepared.response_fact_id),session_id(prepared.session_id),session_generation(prepared.session_generation),selected_node_id(prepared.selected_node_id),selected_choice_id(prepared.selected_choice_id),committed_result_ref(actual),next_node_or_terminal(prepared.next_node_or_terminal){}
InteractionResponseSource::InteractionResponseSource(const InteractionResponseSource&other)
 :copy_memory(response_memory(other)),response_fact_id(other.response_fact_id),session_id(other.session_id),session_generation(other.session_generation),selected_node_id(other.selected_node_id),selected_choice_id(other.selected_choice_id),committed_result_ref(other.committed_result_ref),next_node_or_terminal(other.next_node_or_terminal),tail_consumed_result_ref(other.tail_consumed_result_ref){}
InteractionResponseSource::InteractionResponseSource(InteractionResponseSource&&other){swap(other);}
InteractionResponseSource&InteractionResponseSource::operator=(const InteractionResponseSource&other){if(this!=&other){InteractionResponseSource fresh(other);swap(fresh);}return *this;}
InteractionResponseSource&InteractionResponseSource::operator=(InteractionResponseSource&&other)noexcept{swap(other);return *this;}
void InteractionResponseSource::swap(InteractionResponseSource&other)noexcept{
 if(this==&other)return;static_assert(std::is_nothrow_swappable_v<SourceRef>);static_assert(std::is_nothrow_swappable_v<InteractionResponseNext>);static_assert(std::is_nothrow_swappable_v<std::optional<SourceRef>>);
 using std::swap;copy_memory.swap(other.copy_memory);response_fact_id.swap(other.response_fact_id);session_id.swap(other.session_id);swap(session_generation,other.session_generation);selected_node_id.swap(other.selected_node_id);selected_choice_id.swap(other.selected_choice_id);swap(committed_result_ref,other.committed_result_ref);swap(next_node_or_terminal,other.next_node_or_terminal);tail_consumed_result_ref.swap(other.tail_consumed_result_ref);
}
SelectedResponseTail::SelectedResponseTail(const StableId&session,Integer generation,const SourceRef&source,const InteractionResponseNext&next)
 :copy_memory(tail_memory(session,source,next)),session_id(session),session_generation(generation),response_fact_ref(source),next_node_or_terminal(next){}
SelectedResponseTail::SelectedResponseTail(const SelectedResponseTail&other)
 :copy_memory(tail_memory(other)),session_id(other.session_id),session_generation(other.session_generation),response_fact_ref(other.response_fact_ref),next_node_or_terminal(other.next_node_or_terminal){}
SelectedResponseTail::SelectedResponseTail(SelectedResponseTail&&other){swap(other);}
SelectedResponseTail&SelectedResponseTail::operator=(const SelectedResponseTail&other){if(this!=&other){SelectedResponseTail fresh(other);swap(fresh);}return *this;}
SelectedResponseTail&SelectedResponseTail::operator=(SelectedResponseTail&&other)noexcept{swap(other);return *this;}
void SelectedResponseTail::swap(SelectedResponseTail&other)noexcept{
 if(this==&other)return;static_assert(std::is_nothrow_swappable_v<SourceRef>);static_assert(std::is_nothrow_swappable_v<InteractionResponseNext>);
 using std::swap;copy_memory.swap(other.copy_memory);session_id.swap(other.session_id);swap(session_generation,other.session_generation);swap(response_fact_ref,other.response_fact_ref);swap(next_node_or_terminal,other.next_node_or_terminal);
}
}
