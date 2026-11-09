#pragma once
#include "actor_spatial_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "movement_exact_model.hpp"
#include "resident/storage/actor_location_execution_draft.hpp"
namespace hanmo::resident {
class CommonStore;
class BirthPlacementRead;
class MilitaryActionPlacementRead;
class NamedMilitaryCustodyPlacementRead;
class FieldBattleExitPlacementRead;
struct SpatialCarrierState;
struct TransportRegistrationState;
// Same actual capture payload. Identity handles share the Common producer's
// original objects; this view never clones or traverses their referents.
struct BoundSpatialLocationWriteBuildView {
  std::string_view actor_id;
  SourceRefBuildView actual_source;
  Integer source_schema;
  const MovementAnchor &actual_anchor;
  const std::shared_ptr<const ActorLocationCaptureIdentity> &capture_identity;
  Integer capture_sequence;
  const MovementPrefixState *prefix_witness;
  const std::shared_ptr<const ActorLocationWriteIdentity> &write_identity;
};


struct BoundSpatialLocationWrite {
  CheckpointMemoryCharge copied_fields;
  StableId actor_id;
  SourceRef actual_source;
  Integer source_schema = 0;
  MovementAnchor actual_anchor;
  std::shared_ptr<const ActorLocationCaptureIdentity> capture_identity;
  Integer capture_sequence = 0;
  std::optional<MovementPrefixState> prefix_witness;
  std::shared_ptr<const ActorLocationWriteIdentity> write_identity;
  BoundSpatialLocationWrite() = default;
  explicit BoundSpatialLocationWrite(
      const BoundSpatialLocationWriteBuildView &);
  BoundSpatialLocationWrite(const BoundSpatialLocationWrite &);
  BoundSpatialLocationWrite(BoundSpatialLocationWrite &&);
  BoundSpatialLocationWrite &
  operator=(const BoundSpatialLocationWrite &) = delete;
  BoundSpatialLocationWrite &operator=(BoundSpatialLocationWrite &&) = delete;
};
struct ActualActorSpatialWriteReadBuildView {
  std::string_view actor_id;
  const WorldMoment &occurred_at;
  const MovementAnchor &actual_anchor;
  const CheckpointVector<ActualSpatialSourceBuildView> &immutable_source_head;
};
struct ActualActorSpatialWriteRead;


struct ActualActorSpatialWriteRead {
  CheckpointMemoryCharge copied_fields;
  StableId actor_id;
  WorldMoment occurred_at;
  MovementAnchor actual_anchor;
  std::vector<ActualSpatialSourceRef> immutable_source_head;
  ActualActorSpatialWriteRead() = default;
  explicit ActualActorSpatialWriteRead(
      const ActualActorSpatialWriteReadBuildView &);
  ActualActorSpatialWriteRead(const ActualActorSpatialWriteRead &);
  ActualActorSpatialWriteRead(ActualActorSpatialWriteRead &&);
  ActualActorSpatialWriteRead &
  operator=(const ActualActorSpatialWriteRead &) = delete;
  ActualActorSpatialWriteRead &
  operator=(ActualActorSpatialWriteRead &&) = delete;
};
// Reads one already successful capture member. Does not consume the capture or
// infer a source from today's Actor location. Mutable prefixes are resolved to
// their retained actual effort/activity source at this write's exact M.
Outcome<ActualActorSpatialWriteRead> read_actual_actor_spatial_write(
    const CommonStore &, const MovementExactState &,
    const BoundSpatialLocationWrite &,
    const SpatialCarrierState *carriers = nullptr,
    const TransportRegistrationState *registrations = nullptr,
    ExactArithmeticBudget budget = {},
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);
// 真实S1写入后立即取对应tap；bindings按原执行序覆盖整候选，包括同值location写。
Outcome<BoundSpatialLocationWrite> bind_actual_spatial_location_write(
    const CommonStore &, const MovementExactState &, const StableId &actor_id,
    const SourceRef &, Integer schema, const MovementAnchor &,
    Integer capture_sequence = 0, ExactArithmeticBudget budget = {});
Status validate_spatial_location_capture(
    const CommonStore &, const MovementExactState &,
    const std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {},
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);
Outcome<BoundSpatialLocationWrite> bind_actual_transport_location_write(
    const CommonStore &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const StableId &actor_id, const SourceRef &, const MovementAnchor &,
    Integer capture_sequence = 0, ExactArithmeticBudget budget = {});
Status validate_spatial_location_capture(
    const CommonStore &, const MovementExactState &,
    const SpatialCarrierState &, const TransportRegistrationState &,
    const std::vector<BoundSpatialLocationWrite> &,
    ExactArithmeticBudget budget = {},
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr);
Outcome<BoundSpatialLocationWrite> bind_actual_birth_location_write(
    const CommonStore &, const MovementExactState &, const BirthPlacementRead &,
    const StableId &child_actor_id, const MomentOwnerSource &actual_result,
    const MovementAnchor &, Integer capture_sequence,
    ExactArithmeticBudget budget = {});

Outcome<BoundSpatialLocationWrite> bind_actual_military_action_location_write(
    const CommonStore &, const MovementExactState &,
    const MilitaryActionPlacementRead &, const StableId &actor_id,
    const MomentOwnerSource &actual_application, const MovementAnchor &,
    Integer capture_sequence, ExactArithmeticBudget budget = {});

// 真实期满释放的 Common mutation tap；不借旧军务应用 schema2 冒名。
Outcome<BoundSpatialLocationWrite> bind_actual_named_custody_location_write(
    const CommonStore &, const MovementExactState &,
    const NamedMilitaryCustodyPlacementRead &, const StableId &actor_id,
    const SourceRef &actual_release, const MovementAnchor &,
    Integer capture_sequence, ExactArithmeticBudget budget = {});

// 原野战退出的实际 Common mutation tap；不能借军务完成或只凭退出行作证。
Outcome<BoundSpatialLocationWrite> bind_actual_field_exit_location_write(
    const CommonStore &, const MovementExactState &,
    const FieldBattleExitPlacementRead &, const StableId &actor_id,
    const MomentOwnerSource &actual_exit, const MovementAnchor &,
    Integer capture_sequence, ExactArithmeticBudget budget = {});

// 独立死俘成员交接必须绑定原应用返回的同次 capture 与 write 身份。
// dead 的原 Stable 点与 captured 的原 captor carrier 由完整原来源分别读，
// 调用方不提供自报 anchor，不把 casualty / Capacity 行当实际位置写。
Outcome<BoundSpatialLocationWrite> bind_actual_field_member_loss_location_write(
    const CommonStore &, const MovementExactState &,
    const FieldBattleExitPlacementRead &, const StableId &actor_id,
    const MomentOwnerSource &actual_handoff,
    const std::shared_ptr<const ActorLocationCaptureIdentity> &,
    const std::shared_ptr<const ActorLocationWriteIdentity> &,
    Integer capture_sequence, ExactArithmeticBudget budget = {});

} // namespace hanmo::resident
