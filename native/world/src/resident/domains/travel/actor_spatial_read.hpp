#pragma once
#include "resident/domains/military/field_battle/field_spatial_view.hpp"
#include "resident/domains/military/military_spatial_chart.hpp"
#include "spatial_carrier.hpp"
namespace hanmo::resident {
struct ActualSpatialSourceBuildView {
  SourceRefBuildView source;
  Integer schema = 0;
};
struct ActualSpatialSourceRef {
  SourceRef source;
  Integer schema = 0;
};
// Borrowed inputs for the two original projection-comparison temporaries only.
struct ActorSpatialProjectionAnchorBuildView {
  const Location &location;
  SourceRefBuildView actual_source;
};


// Pure read expression only. MovementAnchor and all persisted wire variants
// remain unchanged; a tactical point never impersonates a world integer anchor.
struct ActualMilitaryActorPosition {
  StableId session_ref;
  Integer generation = 0;
  StableId actual_unit_id;
  const MilitarySpatialChart2 *actual_chart = nullptr;
  MilitarySpatialPoint world_position;
  WorldMoment observed_at;
};
// 连续会战保原局部 dm 与时间；不伪造旧 chart、generation 或世界整数锚点。
using ActorActualPosition =
    std::variant<MovementAnchor, ActualMilitaryActorPosition,
                 FieldBattleSpatialView>;
struct ActorSpatialLocationView {
  StableId actor_id;
  Location location_snapshot;
  ActorActualPosition actual_anchor;
  std::vector<ActualSpatialSourceRef> actual_location_sources;
};
const MovementAnchor *
ordinary_actor_movement_anchor(const ActorSpatialLocationView &) noexcept;
Outcome<const MovementAnchor *>
require_actor_ordinary_movement_anchor(const ActorSpatialLocationView &);
struct ActorTransportParticipation {
  TransportUnitRef transport_unit;
  Integer composition_revision = 0;
  SourceRef registration_source;
  std::optional<StableId> active_journey_id;
};
// Battle位置由S2原唯一局部reader；S1不读Common整数投影推战场位置。
class BattleActorSpatialRead {
public:
  virtual ~BattleActorSpatialRead() = default;
  virtual bool bound_to(const CommonRead &,
                        const MovementExactState &) const noexcept = 0;
  virtual Outcome<ActorSpatialLocationView>
  read_battle_actor_location(const StableId &,
                             const BattleMovementWriter &) const = 0;
};
// 实际连续会战读者只借同一 World；返回空仅代表无绑定，损坏或未部署必须拒绝。
// 非空结果必须由真实 contact/session Common 头提供来源，Location 仅保语义快照。
class FieldBattleActorSpatialRead {
public:
  virtual ~FieldBattleActorSpatialRead() = default;
  virtual bool bound_to(const CommonRead &,
                        const MovementExactState &) const noexcept = 0;
  virtual Outcome<std::optional<ActorSpatialLocationView>>
  read_field_actor_location(const StableId &) const = 0;
};
// 有限两路借用不改变旧 Battle 实例拥有权；legacy ref 构造保原调用兼容。
struct ActorBattleSpatialSources {
  const BattleActorSpatialRead *legacy = nullptr;
  const FieldBattleActorSpatialRead *field = nullptr;
  ActorBattleSpatialSources() = default;
  ActorBattleSpatialSources(const BattleActorSpatialRead &source)
      : legacy(&source) {}
  ActorBattleSpatialSources(const BattleActorSpatialRead *old_source,
                            const FieldBattleActorSpatialRead *field_source)
      : legacy(old_source), field(field_source) {}
};
// 优先真实连续绑定；缺读者、双战场权威或旧读者跨候选均失败，不回退冻结位置。
Outcome<ActorSpatialLocationView> read_actual_actor_spatial_location(
    const CommonRead &, const MovementExactState &,
    const SpatialCarrierSources &, ActorBattleSpatialSources, const StableId &,
    ExactArithmeticBudget budget = {});
// 旧入口只委托有限两路实现；已选 field 的在战人物不会漏回旧世界位置。
Outcome<ActorSpatialLocationView> read_actual_actor_spatial_location(
    const CommonRead &, const MovementExactState &,
    const SpatialCarrierSources &, const BattleActorSpatialRead &,
    const StableId &, ExactArithmeticBudget budget = {});
Outcome<std::optional<ActorTransportParticipation>>
read_actual_transport_participation(const SpatialCarrierState &,
                                    const SpatialCarrierSources &,
                                    const StableId &);
Outcome<bool>
read_actual_transport_participation_presence(const SpatialCarrierState &,
                                             const SpatialCarrierSources &,
                                             const StableId &);
} // namespace hanmo::resident
