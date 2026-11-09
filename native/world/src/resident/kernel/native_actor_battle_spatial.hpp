// 同一 World 的有限战场位置来源；不创建旧六角战场、坐标或第二份位置状态。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/travel/actor_spatial_read.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
// 只能由真实选中的 field/1 根构造；同候选租约持有它和 World 的生命周期。
class NativeFieldBattleActorSpatialRead final
    : public FieldBattleActorSpatialRead,
      public CheckpointAllocatedObject {
  const WorldBundle &world_;
  explicit NativeFieldBattleActorSpatialRead(const WorldBundle &);

public:
  NativeFieldBattleActorSpatialRead(const NativeFieldBattleActorSpatialRead &) =
      delete;
  NativeFieldBattleActorSpatialRead &
  operator=(const NativeFieldBattleActorSpatialRead &) = delete;
  static Outcome<std::unique_ptr<NativeFieldBattleActorSpatialRead>>
  create(const WorldBundle &);
  bool bound_to(const CommonRead &,
                const MovementExactState &) const noexcept override;
  Status validate_bindings() const;
  Outcome<std::optional<ActorSpatialLocationView>>
  read_field_actor_location(const StableId &) const override;
};
// 原目录选型决定两个有限来源是否必需。旧 profile 缺真实 reader 仍拒绝，
// field 未选不要求空实现，已选却缺 state/catalog/reader 不能退回冻结 OnRoute。
Status
validate_native_actor_battle_spatial_sources(const WorldBundle &,
                                             const ActorBattleSpatialSources &);
// 持有真实 field reader，旧 reader 仅借原拥有者；不替旧三个 Original 填壳。
class NativeActorBattleSpatialOwner final : public CheckpointAllocatedObject {
  const WorldBundle &world_;
  std::unique_ptr<NativeFieldBattleActorSpatialRead> field_;
  ActorBattleSpatialSources sources_;
  NativeActorBattleSpatialOwner(const WorldBundle &,
                                const BattleActorSpatialRead *);

public:
  NativeActorBattleSpatialOwner(const NativeActorBattleSpatialOwner &) = delete;
  NativeActorBattleSpatialOwner &
  operator=(const NativeActorBattleSpatialOwner &) = delete;
  static Outcome<std::unique_ptr<NativeActorBattleSpatialOwner>>
  create(const WorldBundle &,
         const BattleActorSpatialRead *actual_legacy = nullptr);
  Status validate_bindings() const;
  const ActorBattleSpatialSources &sources() const noexcept { return sources_; }
};
} // namespace hanmo::resident
