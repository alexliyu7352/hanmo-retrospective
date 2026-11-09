// 中央组合候选；根串行合入。只从本次参数WorldBundle借用权威，不存第二世界。
#pragma once
#include "resident/domains/travel/movement_encounter_integration.hpp"
namespace hanmo::resident {
class CompleteExactWorldBindingOwner;
// 全群体producer装配拥有其provider；本lease只延长具名adapter寿命。
class MotionGroupReadDependencies {public:virtual ~MotionGroupReadDependencies()=default;virtual const GroupMovementContext& group_context()const=0;virtual const MotionContactBusinessRead& contact_sources()const=0;};
class MotionGroupWriteDependencies {public:virtual ~MotionGroupWriteDependencies()=default;virtual const GroupMovementContext& group_context()const=0;virtual MotionContactBusinessSources& contact_sources()=0;};
class MotionCargoReadDependencies {public:virtual ~MotionCargoReadDependencies()=default;virtual const CargoHandlingReadContext& cargo_context()const=0;};
class MotionCargoWriteDependencies {public:virtual ~MotionCargoWriteDependencies()=default;virtual const CargoHandlingWriteContext& cargo_context()const=0;};
Outcome<std::shared_ptr<const MotionExactReadLease>> make_native_motion_read_lease(const WorldBundle&,std::shared_ptr<const MotionGroupReadDependencies> group={},std::shared_ptr<const MotionCargoReadDependencies> cargo={},std::shared_ptr<const PatrolCheckpointOriginalRead> patrol={});
Outcome<std::shared_ptr<MotionExactWriteLease>> make_native_motion_write_lease(WorldBundle&,std::shared_ptr<MotionGroupWriteDependencies> group={},std::shared_ptr<MotionCargoWriteDependencies> cargo={},std::shared_ptr<const PatrolCheckpointOriginalRead> patrol={});
// Complete acquisition only: the enclosing retaining lease keeps owner alive
// beyond the returned inner lease. This path never reacquires a World owner.
Outcome<std::shared_ptr<MotionExactWriteLease>> make_native_motion_write_lease_in_owner(
    WorldBundle&,CompleteExactWorldBindingOwner& owner,
    std::shared_ptr<MotionGroupWriteDependencies> group={},
    std::shared_ptr<MotionCargoWriteDependencies> cargo={},
    std::shared_ptr<const PatrolCheckpointOriginalRead> patrol={});
// 只接受实际未选择group的个人组合；group选中不可借此跳过provider。
MotionExactWorldFactory native_personal_motion_world_factory();
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_travel_action_time_reader(const WorldBundle&,std::shared_ptr<const MotionCargoReadDependencies> cargo={});
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_movement_action_time_reader(const WorldBundle&);
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_cargo_handling_action_time_reader(const WorldBundle&,std::shared_ptr<const MotionCargoReadDependencies>);
}
