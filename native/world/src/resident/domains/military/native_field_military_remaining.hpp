// Field1 只接原任职/成军与真实日历消费者；不提供旧动态、返程或训练的替身。
#pragma once
#include "native_military_world_scope.hpp"
namespace hanmo::resident {
struct NativeFieldMilitaryRemainingDependencies {
  MilitaryCalendarOriginalConsumer *calendar_consumers = nullptr;
  MilitaryCalendarBattleWrite *calendar_battles = nullptr;
  std::function<Status(WorldBundle &,
                       const NativeMilitaryWorldConstructionContext &,
                       ExactPhaseWriteContext &, const StableId &)>
      release_action;
  std::function<Status(
      WorldBundle &, const NativeMilitaryWorldConstructionContext &,
      ExactPhaseWriteContext &, const DynamicMilitaryActionFact2 &, Integer)>
      adopt_formation_locations;
};
// 所有借用由同 Complete owner 保活；factory 不调用 owner.reads() 形成构造回环。
Outcome<std::shared_ptr<NativeMilitaryWorldRemainingOwner>>
make_native_field_military_remaining_owner(
    const WorldBundle &, const CompleteExactWorldBindingOwner &,
    NativeFieldMilitaryRemainingDependencies);
} // namespace hanmo::resident
