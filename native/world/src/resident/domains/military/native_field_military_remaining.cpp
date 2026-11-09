#include "native_field_military_remaining.hpp"
#include "military_exact_validation.hpp"
#include "military_roster_history.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
// 选型来自原目录阶段，不能按空表把已选专业系统藏成空 bindings。
Status selection(const NativeMilitaryWorldConstructionContext &x) {
  const auto &s = x.selected;
  if (!s.field_original || s.dynamic_schema || s.actions.army_return ||
      s.actions.military_train || s.actions.army_resupply || s.naval_repair ||
      s.station || !x.world.field_battle || x.world.military_exact ||
      !x.field_history || !x.world.catalog || !x.world.military_action_time ||
      !x.world.action_elapsed)
    return common_failure("MilitaryFieldRemainingSelectionMismatch");
  return validate_selected_military_field_history(
      x.world.common, x.world.military, x.world.catalog->identity,
      x.field_history);
}
class FieldRemainingScope final : public NativeMilitaryWorldRemainingScope {
  const NativeMilitaryWorldConstructionContext &x_;
  ExactPhaseWriteContext *writes_;
  NativeFieldMilitaryRemainingDependencies dependencies_;
  NativeMilitaryWorldRemainingReadBindings reads_;
  NativeMilitaryWorldRemainingWriteBindings output_;
  std::optional<MilitaryCalendarWriteBinding> calendar_;
  std::optional<ExactMilitaryStateValidationContext> supply_;

public:
  FieldRemainingScope(const NativeMilitaryWorldConstructionContext &x,
                      NativeFieldMilitaryRemainingDependencies dependencies,
                      WorldBundle *world, ExactPhaseWriteContext *writes)
      : x_(x), writes_(writes), dependencies_(std::move(dependencies)) {
    supply_.emplace(ExactMilitaryStateValidationContext{
        *x.world.military_action_time, *x.world.action_elapsed, x.formations,
        x.clock, x.periods, x.office, x.formation, nullptr, nullptr, nullptr,
        x.budget, nullptr, &x.returns, true});
    reads_.field_supply = &*supply_;
    if (!world)
      return;
    calendar_.emplace(world->common, world->military, *world->catalog->military,
                      x.access, dependencies_.calendar_battles,
                      dependencies_.calendar_consumers, true);
    output_.calendar = &*calendar_;
    if (dependencies_.release_action)
      output_.release_action = [this, world](const StableId &id) {
        if (world->active_exact_write_context() != writes_)
          return common_failure("MilitaryFieldRemainingWriteContextChanged");
        return dependencies_.release_action(*world, x_, *writes_, id);
      };
    if (dependencies_.adopt_formation_locations)
      output_.adopt_formation_locations =
          [this, world](const DynamicMilitaryActionFact2 &f, Integer sequence) {
            if (world->active_exact_write_context() != writes_)
              return common_failure(
                  "MilitaryFieldRemainingWriteContextChanged");
            return dependencies_.adopt_formation_locations(*world, x_, *writes_,
                                                           f, sequence);
          };
  }
  bool bound_to(const NativeMilitaryWorldConstructionContext &x,
                const ExactPhaseWriteContext *writes) const noexcept override {
    return &x == &x_ && writes == writes_;
  }
  const NativeMilitaryWorldRemainingReadBindings &
  reads() const noexcept override {
    return reads_;
  }
  NativeMilitaryWorldRemainingWriteBindings *writes() noexcept override {
    return writes_ ? &output_ : nullptr;
  }
};
class FieldRemainingOwner final : public NativeMilitaryWorldRemainingOwner {
  const WorldBundle &world_;
  const CompleteExactWorldBindingOwner &owner_;
  NativeFieldMilitaryRemainingDependencies dependencies_;

public:
  FieldRemainingOwner(const WorldBundle &world,
                      const CompleteExactWorldBindingOwner &owner,
                      NativeFieldMilitaryRemainingDependencies dependencies)
      : world_(world), owner_(owner), dependencies_(std::move(dependencies)) {}
  bool bound_to(
      const WorldBundle &w,
      const CompleteExactWorldBindingOwner &owner) const noexcept override {
    return &w == &world_ && &owner == &owner_;
  }
  Outcome<std::shared_ptr<const NativeMilitaryWorldRemainingScope>>
  read_sources(const NativeMilitaryWorldConstructionContext &x) const override {
    if (!bound_to(x.world, x.owner))
      return {false,
              {},
              common_failure("MilitaryFieldRemainingOwnerMismatch").error};
    auto valid = selection(x);
    if (!valid.ok)
      return {false, {}, valid.error};
    std::shared_ptr<const NativeMilitaryWorldRemainingScope> scope =
        std::make_shared<FieldRemainingScope>(x, dependencies_, nullptr,
                                              nullptr);
    return {true, std::move(scope), {}};
  }
  Outcome<std::shared_ptr<NativeMilitaryWorldRemainingScope>>
  write_sources(WorldBundle &w, const NativeMilitaryWorldConstructionContext &x,
                ExactPhaseWriteContext &writes) override {
    if (&w != &world_ || !bound_to(x.world, x.owner) ||
        w.active_exact_write_context() != &writes)
      return {false,
              {},
              common_failure("MilitaryFieldRemainingWriteOwnerMismatch").error};
    auto valid = selection(x);
    if (!valid.ok)
      return {false, {}, valid.error};
    if ((x.selected.actions.office_accept || x.selected.actions.army_form) &&
        !dependencies_.release_action)
      return {
          false,
          {},
          common_failure("NativeMilitaryActualActionReleaseRequired").error};
    if (x.selected.actions.army_form &&
        !dependencies_.adopt_formation_locations)
      return {false,
              {},
              common_failure("NativeMilitaryActualFormationPlacementRequired")
                  .error};
    std::shared_ptr<NativeMilitaryWorldRemainingScope> scope =
        std::make_shared<FieldRemainingScope>(x, dependencies_, &w, &writes);
    return {true, std::move(scope), {}};
  }
};
} // namespace
Outcome<std::shared_ptr<NativeMilitaryWorldRemainingOwner>>
make_native_field_military_remaining_owner(
    const WorldBundle &w, const CompleteExactWorldBindingOwner &owner,
    NativeFieldMilitaryRemainingDependencies dependencies) {
  if (!w.catalog || !w.catalog->military || !w.field_battle || w.military_exact)
    return {false,
            {},
            common_failure("MilitaryFieldRemainingSelectionMismatch").error};
  auto selected = read_military_exact_phase_selection(*w.catalog);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value.field_original)
    return {false,
            {},
            common_failure("MilitaryFieldRemainingSelectionMismatch").error};
  std::shared_ptr<NativeMilitaryWorldRemainingOwner> value =
      std::make_shared<FieldRemainingOwner>(w, owner, std::move(dependencies));
  return {true, std::move(value), {}};
}
} // namespace hanmo::resident
