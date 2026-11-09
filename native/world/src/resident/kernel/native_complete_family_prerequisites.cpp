#include "native_complete_family_prerequisites.hpp"
#include "native_actor_battle_spatial.hpp"
#include "native_civilian_spatial_owner.hpp"
#include "native_control_continuation.hpp"
#include "native_field_passage.hpp"
#include "native_field_world_scope.hpp"
#include "native_complete_world_core.hpp"
#include "resident/domains/military/military_finite_return_source.hpp"
#include "native_military_state.hpp"
#include "native_military_training_period.hpp"
#include "native_transport_catalog.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue.hpp"
#include "resident/domains/interaction/personal_dialogues/native_personal_dialogue_sources4.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/military_spatial_manifest.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
[[noreturn]] void mismatch(const char *code) {
  throw Failure{common_failure(code, {}, ErrorKind::internal).error};
}
// 原 complete 构造入口仍承担缺实现的具名拒绝，不拿 optional 空值作已实现结果。
[[noreturn]] void missing(const char *reason) {
  auto error =
      common_failure("NOT_IMPLEMENTED", {}, ErrorKind::unsupported_capability)
          .error;
  error.reason = reason;
  throw Failure{std::move(error)};
}
template <class T, class F> Outcome<T> guarded(F &&fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("CompleteFamilyPrerequisiteResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("CompleteFamilyPrerequisiteResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
const StableId *module(const WorldBundle &world, const char *name) {
  const StableId *value = nullptr;
  for (const auto &row : world.catalog->identity.module_versions)
    if (row.first == name) {
      if (value)
        mismatch("CompleteFamilyPrerequisiteDuplicateSelection");
      value = &row.second;
    }
  return value;
}
// 选择依据只来自最终目录模块，目录与拥有根必须精确配对。
template <class T>
void relation_selection(const WorldBundle &world, const char *name,
                        const T *catalog, bool complete) {
  const auto *selected = module(world, name);
  if (selected && *selected != "1")
    missing("CompleteOriginalRelationSchemaOwnerRequired");
  if (bool(selected) != bool(catalog) || bool(selected) != complete ||
      (catalog && !catalog->enabled))
    mismatch("CompleteFamilyPrerequisiteRelationSelectionMismatch");
}
// 目录选择先决定支持闭包；空初态从不豁免所选 reader。
void supported(const NativeCompleteWorldEarlySources &early) {
  const auto &world = early.world;
  if (!world.catalog || !world.catalog->common || !world.catalog->family ||
      !world.catalog->actor_foundation || !world.actor_foundation)
    mismatch("CompleteFamilyPrerequisiteWorldRootsRequired");
  take(validate_native_control_continuation_layout(world));
  relation_selection(world, "family_disciple_legacy",
                     world.catalog->family_disciple_legacy.get(),
                     world.family_disciple_legacy_seed_complete);
  relation_selection(world, "family_biological_legacy",
                     world.catalog->family_biological_legacy.get(),
                     world.family_biological_legacy_seed_complete);
  relation_selection(world, "family_collateral_legacy",
                     world.catalog->family_collateral_legacy.get(),
                     world.family_collateral_legacy_seed_complete);
  if ((!world.catalog->family_disciple_legacy &&
       !world.family_disciple_legacy.commits.empty()) ||
      (!world.catalog->family_biological_legacy &&
       !world.family_biological_legacy.commits.empty()) ||
      (!world.catalog->family_collateral_legacy &&
       (!world.family_collateral_legacy.recognitions.empty() ||
        !world.family_collateral_legacy.recognition_commits.empty() ||
        !world.family_collateral_legacy.legacy_commits.empty())))
    mismatch("CompleteFamilyPrerequisiteUnselectedRelationState");
  const auto transport =
      take(selected_native_transport_versions(world.catalog->identity));
  if (transport.group) {
    if (world.catalog->field_group_passage) {
      take(validate_native_field_army_transport_roots(world));
      if (early.transport_business || !early.formations || !early.returns ||
          !early.periods || !early.field_application_clock)
        mismatch("CompleteFieldArmyOriginalSourcesRequired");
    } else if (!early.transport_business ||
               !world.catalog->household_purchases ||
               world.catalog->selected_household_version != 3)
      missing("CompleteOriginalGroupBusinessOwnerRequired");
  }
  if (!transport.group && early.transport_business)
    mismatch("CompleteUnselectedGroupBusinessOwner");
  const auto military =
      take(selected_native_military_state_versions(world.catalog->identity));
  if (military.dynamic_military)
    missing("CompleteOriginalBattleActorSpatialReadRequired");
  if (world.catalog->actor_foundation->sea_death_rule)
    missing("CompleteOriginalSeaDeathSourceOwnerRequired");
  if (module(world, "craft_jobs"))
    missing("CompleteOriginalCraftJobSourceOwnerRequired");
  if (world.catalog->family_collateral_legacy)
    missing("CompleteOriginalCollateralRecognitionSourceOwnerRequired");
  if (world.catalog->family_disciple_legacy &&
      take(selected_field_battle_version(world.catalog->identity)))
    missing("CompleteOriginalFieldOccupationHistoryRequired");
  if ((world.catalog->family_disciple_legacy ||
       world.catalog->family_biological_legacy) &&
      (!world.control_continuation || !world.catalog->family_legacy ||
       !world.catalog->family_legacy->enabled ||
       world.catalog->family_legacy->version != 4 ||
       !world.catalog->interaction ||
       world.catalog->interaction->common.get() != world.catalog->common.get()))
    mismatch("CompleteFamilyPrerequisiteRelationSelectionMismatch");
}
// 原早期 Army 空间 owner 的长期读口。只在 Core 完成后一次绑定；不存临时
// Field Scope 指针、不回调尚未发布的 Complete getter，也不提前读业务表。
class NativeDeferredFieldFiniteReturnRead final : public MilitaryFiniteReturnSourceRead {
  const NativeCompleteWorldEarlySources &early_;
  const NativeCompleteWorldCoreSources *core_ = nullptr;
public:
  explicit NativeDeferredFieldFiniteReturnRead(const NativeCompleteWorldEarlySources &early)
      : early_(early) {}
  Status bind_original_core(const NativeCompleteWorldCoreSources &core) {
    if (core_ || &core.characters.early != &early_)
      return common_failure("CompleteFieldFiniteReturnCoreBindingMismatch");
    core_ = &core;
    return common_success();
  }
  bool has_original_core() const noexcept { return core_ != nullptr; }
  bool bound_to(const CommonStore &common, const MilitaryState &military,
      const SpatialCarrierState &carriers, const MovementExactState &movement,
      const GroupMovementExecutionState &groups) const noexcept override {
    const auto &w = early_.world;
    return core_ && &core_->characters.early == &early_ &&
        &common == &w.common && &military == &w.military &&
        w.spatial_carriers && &carriers == &*w.spatial_carriers &&
        w.movement_exact && &movement == &*w.movement_exact &&
        w.group_movement_executions && &groups == &*w.group_movement_executions;
  }
  Outcome<MilitaryFiniteReturnOriginalRead>
  read_original_finite_return(const SourceRef &source) const override {
    return guarded<MilitaryFiniteReturnOriginalRead>([&] {
      const auto &w = early_.world;
      if (!core_ || &core_->characters.early != &early_ ||
          !w.catalog->field_group_passage)
        mismatch("CompleteFieldFiniteReturnOriginalCoreRequired");
      // 本栈 Scope 持自己的具体 reader；返回值拥有所有字符串/列表，不能泄露
      // 栈中借口。这里不调用 Scope::validate 的全表反扫。
      auto scope = take(NativeFieldWorldScope::create_from_original_core(w, *core_));
      const auto *reader = scope->finite_returns();
      if (!reader || reader == this)
        mismatch("CompleteFieldFiniteReturnOriginalReaderRequired");
      return take(reader->read_original_finite_return(source));
    });
  }
  // 稳定到点与真开程同样只读实际已采用链；返回拥有值，不泄栈中终点指针。
  Outcome<MilitaryFiniteReturnOriginalRead>
  read_actual_finite_return_continuation(const SourceRef &source) const override {
    return guarded<MilitaryFiniteReturnOriginalRead>([&] {
      const auto &w = early_.world;
      if (!core_ || &core_->characters.early != &early_ ||
          !w.catalog->field_group_passage)
        mismatch("CompleteFieldFiniteReturnOriginalCoreRequired");
      auto scope = take(NativeFieldWorldScope::create_from_original_core(w, *core_));
      const auto *reader = scope->finite_returns();
      if (!reader || reader == this)
        mismatch("CompleteFieldFiniteReturnOriginalReaderRequired");
      return take(reader->read_actual_finite_return_continuation(source));
    });
  }
};
class NativeFamilyPrerequisites final
    : public NativeCompleteFamilyPrerequisiteOwner {
  const NativeCompleteWorldEarlySources &early_;
  std::unique_ptr<NativeCivilianSpatialOwner> civilian_;
  std::unique_ptr<NativeActorBattleSpatialOwner> battles_;
  std::optional<MilitaryFieldHistoryRead> field_history_;
  NativeDeferredFieldFiniteReturnRead finite_returns_;
  std::unique_ptr<NativeArmySpatialCarrierSources> army_;
  NativeCompleteFamilyPrerequisiteExports exports_;
  const NativeCharacterFamilyWorldRead *family_ = nullptr;
  bool bound_ = false;
  std::optional<InteractionAccess> interaction_;
  std::optional<NativePersonalDialogueSources4> dialogue_sources_;
  std::optional<BornPersonalDialogueAccess> born_dialogues_;
  std::optional<DiscipleControlRelationContext> disciple_;
  std::optional<BiologicalControlDecisionContext> biological_;

  // 已有 Complete stamp 门的同一有限额度；不创建第二预算或缓存原时刻。
  explicit NativeFamilyPrerequisites(
      const NativeCompleteWorldEarlySources &early)
      : early_(early),
        civilian_(take(NativeCivilianSpatialOwner::create(
            early.world, {4096, 4096, 1024 * 1024}, early.transport_business,
            nullptr, early.formations, early.returns, early.periods))),
        battles_(take(NativeActorBattleSpatialOwner::create(early.world))),
        field_history_(
            early.world.catalog->field_group_passage
                ? std::optional<
                      MilitaryFieldHistoryRead>{MilitaryFieldHistoryRead{
                      *early.world.field_battle,
                      *early.field_application_clock}}
                : std::nullopt),
        finite_returns_(early),
        army_(field_history_
                  ? std::make_unique<NativeArmySpatialCarrierSources>(
                        early.world.common, early.world.military,
                        *early.world.catalog->military,
                        MilitarySelectedHistoryRead{
                            early.world.catalog->identity,
                            early.world.military_exact
                                ? &*early.world.military_exact
                                : nullptr,
                            &*field_history_},
                        *early.world.spatial_carriers,
                        *early.world.movement_exact, civilian_->original(),
                        civilian_->stamps(),
                        early.world.population_military_enabled()
                            ? &early.world.population_military
                            : nullptr,
                        early.world.population_military_enabled()
                            ? early.world.catalog->population_military.get()
                            : nullptr,
                        early.formations, early.returns, early.periods,
                        &*early.world.group_movement_executions, &finite_returns_)
                  : nullptr),
        exports_{army_ ? static_cast<const SpatialCarrierSources &>(*army_)
                       : civilian_->original(),
                 battles_->sources()} {}

  // 调用方原业务已核 payload.tick；此处仍要求真实 Common 头并读同一控制链。
  Outcome<StableId> player_before(Integer tick, const StableId &id) const {
    return guarded<StableId>([&] {
      if (!family_ || tick < 0 || tick > early_.world.common.tick())
        mismatch("CompleteRelationControlBoundaryRequired");
      const auto *head = early_.world.common.state().facts.find(id);
      if (!head || head->id != id)
        mismatch("CompleteRelationControlBoundaryRequired");
      return take(read_profile_control_before_fact(
                      family_->control(), {head->domain, head->kind, id}))
          .actor_id;
    });
  }

public:
  // 仅中央 Core emplace 后、领域构造前调用；不授予新读写权限。
  Status bind_original_core(const NativeCompleteWorldCoreSources &core) {
    if (!bound_ || !family_ || family_ != &core.characters.family ||
        &core.characters.early != &early_ ||
        &core.characters.spatial.original != &exports_.spatial_original ||
        !army_ || &exports_.spatial_original != army_.get())
      return common_failure("CompleteFieldFiniteReturnCoreBindingMismatch");
    return finite_returns_.bind_original_core(core);
  }
  static Outcome<std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>>
  create(const NativeCompleteWorldEarlySources &early) {
    return guarded<std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>>([&] {
      supported(early);
      auto out = std::unique_ptr<NativeFamilyPrerequisites>(
          new NativeFamilyPrerequisites(early));
      take(out->validate_early_bindings(early));
      return std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>(
          std::move(out));
    });
  }
  bool bound_to(const WorldBundle &world) const noexcept override {
    return &early_.world == &world;
  }
  Status validate_early_bindings(
      const NativeCompleteWorldEarlySources &early) const override {
    return guarded<Done>([&] {
      if (&early != &early_ || !civilian_ || !battles_ ||
          !civilian_->bound_to(early.world) ||
          &exports_.spatial_original !=
              (army_ ? static_cast<const SpatialCarrierSources *>(army_.get())
                     : &civilian_->original()) ||
          bool(army_) != bool(early.world.catalog->field_group_passage) ||
          bool(field_history_) != bool(army_) ||
          exports_.battle_spatial.legacy != battles_->sources().legacy ||
          exports_.battle_spatial.field != battles_->sources().field)
        mismatch("CompleteFamilyPrerequisiteEarlyBindingMismatch");
      supported(early);
      take(civilian_->validate_bindings());
      take(battles_->validate_bindings());
      return Done{};
    });
  }
  // 私有装配栈仅调用一次；这里不调用 Family::validate_bindings 形成回环。
  Status
  bind_family_sources(const NativeCharacterFamilyWorldRead &family) override {
    return guarded<Done>([&] {
      if (bound_ || family_ || !family.people().bound_to(early_.world.common) ||
          &family.control().common != &early_.world.common)
        mismatch("CompleteFamilyPrerequisiteLateBindingMismatch");
      const auto &world = early_.world;
      family_ = &family;
      if (world.catalog->family_disciple_legacy) {
        // Field 选型已在早门拒缺历史；此原口只证明既有军史可用性，不拿 presence
        // 代替。
        ApprenticeshipIdentityRead people;
        people.births = family.born() ? &world.family_birth : nullptr;
        people.complete_people = &family.people();
        people.player_before = [this](Integer tick, const StableId &id) {
          return player_before(tick, id);
        };
        people.personally_available = world.family_legacy_playable_read();
        disciple_.emplace(DiscipleControlRelationContext{
            world.common, *world.catalog->common, world.family_apprenticeship,
            *world.catalog->family_legacy,
            *world.catalog->family_disciple_legacy, std::move(people)});
        take(validate_disciple_control_catalog(
            *world.catalog->family_disciple_legacy,
            *world.catalog->family_legacy, *world.catalog->common,
            *world.catalog->interaction));
        exports_.disciple = &*disciple_;
      }
      if (world.catalog->family_biological_legacy) {
        if (!world.catalog->family_lineage ||
            !world.catalog->family_lineage->enabled)
          mismatch("CompleteBiologicalLineageRootsRequired");
        bool needs_born = false;
        for (const auto &row :
             world.catalog->family_biological_legacy->definitions.rows())
          needs_born |= row.second.meeting_kind ==
                        BiologicalControlMeetingKind::born_personal_template;
        if (needs_born) {
          if (!world.catalog->born_personal_dialogues ||
              !world.interaction.born_personal_dialogues || !family.born())
            missing("CompleteOriginalBornPersonalDialogueSourcesRequired");
          interaction_.emplace(world.services.interaction_access());
          dialogue_sources_.emplace(world.common, world.interaction,
                                    family.people(), family.control(),
                                    family.born());
          take(dialogue_sources_->validate_bindings());
          born_dialogues_.emplace(world.common, world.interaction,
                                  *world.interaction.born_personal_dialogues,
                                  *world.catalog->interaction,
                                  *world.catalog->born_personal_dialogues,
                                  *interaction_,
                                  dialogue_sources_->born_sources());
        }
        BiologicalControlTemplateRead templates;
        if (needs_born)
          templates = [this](const StableId &id)
              -> Outcome<BiologicalControlTemplateIdentity> {
            const auto *row = born_personal_dialogue_template(
                *early_.world.catalog->born_personal_dialogues, id);
            if (!row)
              return {
                  false,
                  {},
                  common_failure("BiologicalControlTemplateSourceRequired", id)
                      .error};
            return {true, {row->id, row->prototype_id, row->dialogue_id}, {}};
          };
        biological_.emplace(BiologicalControlDecisionContext{
            {world.common,
             *world.catalog->common,
             *world.catalog->family_lineage,
             family.born() ? &world.family_birth : nullptr,
             &world.family_life,
             {},
             &family.people()},
            *world.catalog->family_biological_legacy,
            *world.catalog->family_legacy,
            *world.catalog->interaction,
            std::move(templates),
            born_dialogues_ ? &*born_dialogues_ : nullptr});
        take(validate_biological_control_context(*biological_));
        exports_.biological = &*biological_;
      }
      bound_ = true;
      return Done{};
    });
  }
  Status validate_bindings(
      const NativeCompleteWorldEarlySources &early) const override {
    return guarded<Done>([&] {
      take(validate_early_bindings(early));
      if (!bound_ || !family_ ||
          bool(early.world.catalog->family_disciple_legacy) !=
              bool(disciple_) ||
          bool(early.world.catalog->family_biological_legacy) !=
              bool(biological_) ||
          exports_.disciple != (disciple_ ? &*disciple_ : nullptr) ||
          exports_.biological != (biological_ ? &*biological_ : nullptr) ||
          exports_.collateral)
        mismatch("CompleteFamilyPrerequisiteLateBindingMismatch");
      if ((disciple_ &&
           disciple_->people.complete_people != &family_->people()) ||
          (biological_ &&
           biological_->people.complete_people != &family_->people()))
        mismatch("CompleteFamilyPrerequisitePeopleMismatch");
      if (dialogue_sources_)
        take(dialogue_sources_->validate_bindings());
      return Done{};
    });
  }
  const NativeCompleteFamilyPrerequisiteExports &
  sources() const noexcept override {
    return exports_;
  }
};
} // namespace
// 具名原 factory 的一次晚绑定；错 owner 明确拒绝，不能把任意基类硬转为真图。
Status bind_native_complete_family_original_core(
    NativeCompleteFamilyPrerequisiteOwner &owner, const NativeCompleteWorldCoreSources &core) {
  auto *actual = dynamic_cast<NativeFamilyPrerequisites *>(&owner);
  if (!actual || !actual->bound_to(core.characters.early.world))
    return common_failure("CompleteFieldFiniteReturnOwnerRequired");
  return actual->bind_original_core(core);
}
Outcome<std::unique_ptr<NativeCompleteFamilyPrerequisiteOwner>>
make_native_complete_family_prerequisites(
    const NativeCompleteWorldEarlySources &early) {
  return NativeFamilyPrerequisites::create(early);
}
} // namespace hanmo::resident
