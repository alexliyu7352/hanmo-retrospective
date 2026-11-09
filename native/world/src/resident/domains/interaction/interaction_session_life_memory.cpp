#include "interaction_response_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <utility>
namespace hanmo::resident {
namespace {
CheckpointCopyLease reserve_copy(const InteractionSessionLifeSource&source){

 return CheckpointCopyLease::reserve();
}
}
InteractionSessionLifeSource::InteractionSessionLifeSource(const InteractionSessionLifeSource&other)
 :copy_memory(reserve_copy(other)),death_ref(other.death_ref),session_id(other.session_id),role(other.role),effect(other.effect){}
InteractionSessionLifeSource::InteractionSessionLifeSource(InteractionSessionLifeSource&&other)
 :copy_memory(other.copy_memory),death_ref(std::move(other.death_ref)),session_id(std::move(other.session_id)),role(other.role),effect(std::move(other.effect)){
 other.copy_memory={};
}
InteractionSessionLifeSource&InteractionSessionLifeSource::operator=(const InteractionSessionLifeSource&other){
 if(this!=&other){InteractionSessionLifeSource fresh(other);swap(fresh);}return *this;
}
InteractionSessionLifeSource&InteractionSessionLifeSource::operator=(InteractionSessionLifeSource&&other){
 if(this!=&other){InteractionSessionLifeSource fresh(std::move(other));swap(fresh);}return *this;
}
void InteractionSessionLifeSource::swap(InteractionSessionLifeSource&other){
 if(this==&other)return;
 auto left=copy_memory,right=other.copy_memory;
 auto both=CheckpointCopyLease::hold_both(left,right);
 copy_memory=both;other.copy_memory=both;
 // ActorFoundationAt contains exact values whose move/swap may allocate. If
 // one throws, both partially moved private rows retain both original leases.
 using std::swap;
 swap(death_ref,other.death_ref);swap(session_id,other.session_id);
 swap(role,other.role);swap(effect,other.effect);
 copy_memory=std::move(right);other.copy_memory=std::move(left);
}
}
