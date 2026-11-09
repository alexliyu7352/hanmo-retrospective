// Actual-M military construction. The enclosing complete owner retains the
// borrowed core; each acquisition owns its snapshots and original contexts.
#pragma once
#include "military_exact_phase.hpp"
#include "native_formation_original.hpp"
#include "native_office_original.hpp"
#include "resident/kernel/native_complete_world_registry.hpp"
#include <memory>
namespace hanmo::resident {
class NativeCharacterFamilyWorldRead;
class NativeMilitaryTrainingPeriodRead;
struct WorldBundle;

// A stable, synchronous construction input, owned by the returned military
// scope. A professional factory may copy its references into its returned
// owning scope; it must not cache them in the long-lived source owner.
struct NativeMilitaryWorldConstructionContext {
  const WorldBundle &world;
  const CompleteExactWorldBindingOwner &owner;
  const WorldMoment &at;
  const CurrentMomentRead &clock;
  const MilitaryAccess &access;
  const CharacterConditionSources &body;
  const NativeCharacterFamilyWorldRead &family;
  const MilitaryExactPhaseSelection &selected;
  const ExactOfficeReadContext *office;
  const ExactFormationReadContext *formation;
  const MilitaryAuthoritySourceContext *authority;
  const MilitaryFormationSourceRead &formations;
  const MilitaryReturnSourceRead &returns;
  const ExactTrainingPeriodRead &periods;
  ExactArithmeticBudget budget{};
  // 真 Field 原段来源由同一 scope 保活，供专业 Return context 借用。
  const MilitaryFieldHistoryRead *field_history = nullptr;
};
// Only the still-specialized original source constructors are supplied here.
// These are their genuine existing typed contexts, not success/Required shims.
struct NativeMilitaryWorldRemainingReadBindings {
  const ExactTrainingReadContext *training = nullptr;
  const ExactReturnReadContext *returning = nullptr;
  const ExactResupplyReadContext *resupply = nullptr;
  const ExactNavalRepairReadContext *naval_repair = nullptr;
  const DynamicMilitaryVerifiedSourceContext *dynamic_sources = nullptr;
  const MilitaryDutyContext *duty = nullptr;
  // Only needed when authority is selected but neither local Office nor
  // Formation context is selected. The returned remaining scope owns this
  // actual context.
  const MilitaryAuthorityTimeContext *authority_time = nullptr;
  const ExactMilitaryStateValidationContext *field_supply = nullptr;
};
struct NativeMilitaryWorldRemainingWriteBindings {
  const ExactTrainingWriteContext *training = nullptr;
  const ExactReturnWriteContext *returning = nullptr;
  const ExactResupplyWriteContext *resupply = nullptr;
  const ExactNavalRepairWriteContext *naval_repair = nullptr;
  const DynamicMilitaryGroupWriteContext *dynamic_sources = nullptr;
  MilitaryCalendarWriteBinding *calendar = nullptr;
  // Actual S0/S1 consumers supplied by the same mutable candidate owner.
  std::function<Status(const StableId &)> release_action;
  std::function<Status(const DynamicMilitaryActionFact2 &, Integer)>
      adopt_formation_locations;
};
class NativeMilitaryWorldRemainingScope : public CheckpointAllocatedObject {
public:
  NativeMilitaryWorldRemainingScope() = default;
  virtual ~NativeMilitaryWorldRemainingScope() = default;
  NativeMilitaryWorldRemainingScope(const NativeMilitaryWorldRemainingScope &) =
      delete;
  NativeMilitaryWorldRemainingScope &
  operator=(const NativeMilitaryWorldRemainingScope &) = delete;
  NativeMilitaryWorldRemainingScope(NativeMilitaryWorldRemainingScope &&) =
      delete;
  NativeMilitaryWorldRemainingScope &
  operator=(NativeMilitaryWorldRemainingScope &&) = delete;
  virtual bool bound_to(const NativeMilitaryWorldConstructionContext &,
                        const ExactPhaseWriteContext *) const noexcept = 0;
  virtual const NativeMilitaryWorldRemainingReadBindings &
  reads() const noexcept = 0;
  virtual NativeMilitaryWorldRemainingWriteBindings *writes() noexcept = 0;
};
// An owning, explicitly typed late-original boundary. No implementation is
// fabricated here when a selected professional reader/writer is unavailable.
class NativeMilitaryWorldRemainingOwner : public CheckpointAllocatedObject {
public:
  NativeMilitaryWorldRemainingOwner() = default;
  virtual ~NativeMilitaryWorldRemainingOwner() = default;
  NativeMilitaryWorldRemainingOwner(const NativeMilitaryWorldRemainingOwner &) =
      delete;
  NativeMilitaryWorldRemainingOwner &
  operator=(const NativeMilitaryWorldRemainingOwner &) = delete;
  NativeMilitaryWorldRemainingOwner(NativeMilitaryWorldRemainingOwner &&) =
      delete;
  NativeMilitaryWorldRemainingOwner &
  operator=(NativeMilitaryWorldRemainingOwner &&) = delete;
  virtual bool
  bound_to(const WorldBundle &,
           const CompleteExactWorldBindingOwner &) const noexcept = 0;
  virtual Outcome<std::shared_ptr<const NativeMilitaryWorldRemainingScope>>
  read_sources(const NativeMilitaryWorldConstructionContext &) const = 0;
  virtual Outcome<std::shared_ptr<NativeMilitaryWorldRemainingScope>>
  write_sources(WorldBundle &, const NativeMilitaryWorldConstructionContext &,
                ExactPhaseWriteContext &) = 0;
};
struct NativeMilitaryWorldScopeDependencies {
  const CompleteExactWorldBindingOwner &owner;
  const NativeCharacterFamilyWorldRead &family;
  const MilitaryAccess &access;
  const NativeMilitaryTrainingPeriodRead &periods;
  const MilitaryFormationSourceRead &formations;
  const MilitaryReturnSourceRead &returns;
  const MilitaryActionPlacementRead &placements;
  const SpatialCarrierSources &spatial;
  const NativeActorSocialOccupancyRead &social;
  const BirthPlacementRead *births = nullptr;
  const NavalHistoryAccess *naval_history = nullptr;
  std::shared_ptr<NativeMilitaryWorldRemainingOwner> remaining;
  ExactArithmeticBudget budget{};
};
// The returned scope owns at by value, a true World clock, the Body snapshot,
// its Access copy and both actual OriginalRead implementations. It never calls
// owner.reads() or re-acquires a public complete-world factory while building.
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_military_world_read_scope(const WorldBundle &,
                                      NativeMilitaryWorldScopeDependencies,
                                      const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_military_world_write_scope(WorldBundle &,
                                       NativeMilitaryWorldScopeDependencies,
                                       const WorldMoment &,
                                       ExactPhaseWriteContext &);
} // namespace hanmo::resident
