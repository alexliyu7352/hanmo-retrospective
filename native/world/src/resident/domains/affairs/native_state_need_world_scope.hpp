// Same-candidate, actual-M construction of the original S3/S4/S10 coordinator.
#pragma once
#include "state_need_policy_read.hpp"
#include "resident/kernel/native_complete_world_registry.hpp"
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
struct CharacterConditionSources;
class CurrentMomentRead;
struct NativeStateNeedWorldConstructionContext {
 const WorldBundle& world;
 const NativeCompleteWorldCoreSources& core;
 const WorldMoment& at;
 const CurrentMomentRead& clock;
 const CharacterConditionSources& body;
};
struct NativeStateNeedWorldOriginalExports {
 const StateTaskSourceRead& task_sources;
 const CareRequirementCatalog* care_catalog=nullptr;
 const CareGoalSourceContext* care=nullptr;
 const MedicalStateNeedPolicyRead* medical=nullptr;
 const StateGoalRead* medical_goals=nullptr;
};
// The actual Care/Medical goal and S3 source owners live here. This is an
// owning construction boundary, not an alternate goal or settlement provider.
class NativeStateNeedWorldOriginalScope:public CheckpointAllocatedObject {
public:
 NativeStateNeedWorldOriginalScope()=default;
 virtual ~NativeStateNeedWorldOriginalScope()=default;
 NativeStateNeedWorldOriginalScope(const NativeStateNeedWorldOriginalScope&)=delete;
 NativeStateNeedWorldOriginalScope&operator=(const NativeStateNeedWorldOriginalScope&)=delete;
 NativeStateNeedWorldOriginalScope(NativeStateNeedWorldOriginalScope&&)=delete;
 NativeStateNeedWorldOriginalScope&operator=(NativeStateNeedWorldOriginalScope&&)=delete;
 virtual bool bound_to(const NativeStateNeedWorldConstructionContext&,bool writable)const noexcept=0;
 virtual const NativeStateNeedWorldOriginalExports& sources()const noexcept=0;
 virtual StateTaskSourceRead* mutable_task_sources()noexcept=0;
};
class NativeStateNeedWorldOriginalOwner:public CheckpointAllocatedObject {
public:
 NativeStateNeedWorldOriginalOwner()=default;
 virtual ~NativeStateNeedWorldOriginalOwner()=default;
 NativeStateNeedWorldOriginalOwner(const NativeStateNeedWorldOriginalOwner&)=delete;
 NativeStateNeedWorldOriginalOwner&operator=(const NativeStateNeedWorldOriginalOwner&)=delete;
 NativeStateNeedWorldOriginalOwner(NativeStateNeedWorldOriginalOwner&&)=delete;
 NativeStateNeedWorldOriginalOwner&operator=(NativeStateNeedWorldOriginalOwner&&)=delete;
 virtual bool bound_to(const WorldBundle&)const noexcept=0;
 virtual Outcome<std::shared_ptr<const NativeStateNeedWorldOriginalScope>> read_sources(const NativeStateNeedWorldConstructionContext&)const=0;
 virtual Outcome<std::shared_ptr<NativeStateNeedWorldOriginalScope>> write_sources(WorldBundle&,const NativeStateNeedWorldConstructionContext&)=0;
};
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>> make_native_state_need_world_read_scope(
 const WorldBundle&,const NativeCompleteWorldCoreSources&,std::shared_ptr<NativeStateNeedWorldOriginalOwner>,const WorldMoment&);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>> make_native_state_need_world_write_scope(
 WorldBundle&,const NativeCompleteWorldCoreSources&,std::shared_ptr<NativeStateNeedWorldOriginalOwner>,const WorldMoment&);
}
