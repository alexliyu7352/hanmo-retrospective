#pragma once
#include "native_action_time_sources.hpp"
#include "exact_time_profile.hpp"
#include "resident/domains/tasks/work_execution/work_exact_runtime.hpp"
namespace hanmo::resident {
struct WorldBundle;
class MotionCargoReadDependencies;
class CharacterConditionWorldRead;class CompleteExactWorldBindingOwner;
// All supplemental leases are constructed by original domain factories against this same WorldBundle.
// Every selected non-core domain must be present; this function does not substitute an empty owner.
// original_body borrows the actual same-World body owner. The enclosing complete graph
// must retain it for the entire lifetime of the returned source. No body is reconstructed;
// the original Task factory still requires it for selected Task execution version 2.
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_action_time_sources(
 const WorldBundle&,const CompleteExactTimeRuntime&,CheckpointVector<CompiledActionTimeOwnerBinding> original_professional_owners,TaskWorkWorldFactory original_task_work,std::shared_ptr<const MotionCargoReadDependencies> original_cargo,const CharacterConditionWorldRead* original_body=nullptr);
// The same original registry/assembler, with Task/Work reads borrowing the
// enclosing complete owner instead of invoking its public factory recursively.
// owner and all borrowed roots must outlive this returned source reader.
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_action_time_sources_in_owner(
 const WorldBundle&,const CompleteExactTimeRuntime&,CheckpointVector<CompiledActionTimeOwnerBinding> original_professional_owners,const CompleteExactWorldBindingOwner&,std::shared_ptr<const MotionCargoReadDependencies> original_cargo,const CharacterConditionWorldRead* original_body=nullptr);
}
