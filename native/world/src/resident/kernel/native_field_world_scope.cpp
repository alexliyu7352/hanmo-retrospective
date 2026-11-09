#include "native_field_world_scope.hpp"
#include "exact_complete_world.hpp"
#include "exact_time_profile.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_group_world.hpp"
#include "native_military_occupation.hpp"
#include "native_military_state.hpp"
#include "native_military_training_period.hpp"
#include "native_political_relations.hpp"
#include "native_transport_catalog.hpp"
#include "native_movement_boundary_continuation_catalog.hpp"
#include "resident/domains/travel/movement_boundary_continuation_source.hpp"
#include "original_world_read_boundary.hpp"
#include "political_takeover_integration.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_selection.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/actor_foundation/named_custody_availability_internal.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/military/field_battle/field_command_source.hpp"
#include "resident/domains/military/field_battle/field_army_exit_selection.hpp"
#include "resident/domains/military/field_battle/field_army_exit_return.hpp"
#include "resident/domains/military/field_battle/field_army_exit_validation.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ordinary_loss.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_exit_placement.hpp"
#include "resident/domains/military/field_battle/field_roster_origin.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/domains/military/military_authority_time.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
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
[[noreturn]] void required(const char *reason) {
  throw Failure{
      common_failure(reason, {}, ErrorKind::unsupported_capability).error};
}
template <class T, class F> Outcome<T> guarded(F &&function) {
  try {
    return {true, function(), {}};
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldOriginalScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("FieldOriginalScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
// 冷导出/shape 与整世界验证共用此来源门；只在原完整 owner 真正借用处
// 自持同 World 当刻票，已有外票只借不替换。缺根仍交原 owner 拒绝。
std::unique_ptr<PhaseCurrentReadScope>
own_field_source_validation_phase(const WorldBundle &world) {
  if (!world.catalog->exact_time ||
      !exact_complete_selected(*world.catalog->exact_time) ||
      world.has_active_read_phase() || !world.read_stamps)
    return {};
  const auto now = take(world.actual_phase_moment());
  const auto names_memory = CheckpointMemoryCharge(0);
  const StableId phase_name{"field_source_validation"};
  const StableId provider_name{"field_battle"};
  return take(world.enter_phase_read_context(
      phase_name, provider_name, {now, now}, now));
}
// 原 CampaignDepart/2 的有限军事 wire 版本来自真实目录，不以现有军团反推选择。
MilitaryMarchAuthorSelection
original_march_selection(const WorldBundle &world) {
  take(validate_native_military_occupation_catalog(*world.catalog));
  const auto &actual = world.common.state().identity;
  const CheckpointIdentity identity{actual.world_id, actual.revision,
                                    actual.tick, world.catalog->identity};
  const auto domains = take(checkpoint_domains_from_identity(identity));
  std::optional<Integer> physical;
  for (const auto &domain : domains)
    if (domain.name == "military") {
      if (physical)
        required("FieldOriginalMilitaryMarchSelectionAmbiguous");
      physical = domain.codec_version;
    }
  if (!physical || *physical < 67 || *physical > 113)
    required("FieldOriginalMilitaryMarchSelectionRequired");
  const auto schema = take(military_checkpoint_base_version(
      static_cast<std::uint32_t>(*physical),
      world.catalog->selected_military_occupation_schema));
  return {"original_campaign_departure/2", schema};
}
// 不调用任何完整 Field reader，先证明被借 owner 与根、时刻确为本候选。
CharacterConditionSources
original_body(const WorldBundle &world,
              const NativeCompleteWorldCoreSources &core) {
  if (!world.catalog || !world.catalog->common || !world.catalog->military ||
      !world.catalog->family || !world.catalog->growth ||
      !world.catalog->actor_foundation || !world.catalog->growth_permanent ||
      !world.catalog->field_battle || !world.catalog->travel ||
      !world.actor_foundation || !world.growth_permanent ||
      !world.field_battle || !world.world_timeline)
    required("FieldOriginalSameWorldRootsRequired");
  const auto &early = core.characters.early;
  if (&early.world != &world || !early.formations || !early.returns ||
      !early.periods || !early.field_application_clock ||
      !early.original_read_boundary || !early.attribute_absence ||
      !early.field_application_clock->bound_to(world.common, world.military,
                                               *world.field_battle) ||
      !early.formations->bound_to(world.common.state(), world.military))
    required("FieldOriginalCompleteOwnerRequired");
  auto body = take(core.characters.family.body().sources());
  if (&body.common != &world.common ||
      &body.state != &*world.actor_foundation ||
      &body.catalog != world.catalog->actor_foundation.get() ||
      body.shared != world.catalog->common.get() ||
      body.growth != &world.growth ||
      body.growth_catalog != world.catalog->growth.get() ||
      body.military != &world.military ||
      body.military_exact !=
          (world.military_exact ? &*world.military_exact : nullptr) ||
      body.permanent_growth != &*world.growth_permanent ||
      body.permanent_growth_catalog != world.catalog->growth_permanent.get() ||
      body.work_practice != &core.characters.family.work_practice())
    required("FieldOriginalBodyRootMismatch");
  const auto at = take(world.actual_phase_moment());
  if (take(compare_world_moments(body.actual_now, at)) ||
      take(
          compare_world_moments(take(early.clock.read_current_moment()), at)) ||
      take(compare_world_moments(
          take(early.field_application_clock->read_current_moment()), at)))
    required("FieldOriginalActualMomentMismatch");
  return body;
}
// 未装实际 Army Group 来源图时拒本新增持久形状和孤头；不是空 provider 成功。
Status reject_unselected_army_exit_sources(const WorldBundle &w) {
  if (w.field_battle && !w.field_battle->army_exit_plans.empty())
    return common_failure("FieldArmyExitOriginalFieldSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  for (const auto &[id, head] : w.common.state().facts.rows())
    if (head.kind == "field_army_exit_choice" || head.kind == "field_army_exit_step" ||
        head.kind == "field_army_exit_ready")
      return common_failure("FieldArmyExitOriginalFieldSourcesRequired", id,
                            ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace

// 成员顺序也是寿命合同：值快照先于引用它的上下文，具体读口先于 abilities。
struct NativeFieldWorldScope::Impl final : CheckpointAllocatedObject {
  const WorldBundle &world;
  const NativeCompleteWorldCoreSources &core;
  CharacterConditionSources body;
  GrowthPermanentSources growth;
  FieldOriginalRosterReadContext rosters;
  std::unique_ptr<PoliticalTakeoverWorldAccess> territory;
  std::optional<PoliticalRelationsReadContext> relations;
  // 原精确任职历史先于所有 march 借口构成，后于全部 march 借口释放；不调用完整军事 owner。
  std::optional<MilitaryAuthoritySourceContext> march_authority;
  std::optional<MilitaryMarchAuthorSelection> march;
  std::optional<FieldExactContactReadContext> contacts;
  std::optional<FieldGroupHandoffReadContext> handoffs;
  std::optional<OriginalFieldBattleExitPlacementRead> placements;
  // 同一 Field 原时钟的显式读边界，内部仍由真实 World/S0 证明观察时刻。
  OriginalWorldReadBoundary field_boundary;
  FieldBattleAbilityReadContext abilities;
  // 后于全部借用上下文构造，早于它们析构；构造时不读取候选半成品。
  std::optional<FieldArmyExitFiniteReturnRead> finite_returns;

  Impl(const WorldBundle &w,
       const NativeCompleteWorldCoreSources &original_core,
       CharacterConditionSources actual_body, const DynamicStepInput *phase)
      : world(w), core(original_core), body(std::move(actual_body)),
        growth{body, *w.catalog->growth_permanent, *w.growth_permanent,
               core.characters.family.work_practice()},
        rosters{w.common,
                w.military,
                w.military_exact ? &*w.military_exact : nullptr,
                w.catalog->identity,
                *core.characters.early.formations,
                *core.characters.early.returns,
                *core.characters.early.periods,
                core.characters.early.clock,
                {},
                w.population_military_enabled() ? &w.population_military
                                                : nullptr,
                w.population_military_enabled()
                    ? w.catalog->population_military.get()
                    : nullptr,
                core.characters.early.field_application_clock},
        field_boundary(w, *core.characters.early.field_application_clock),
        abilities{*w.catalog->common, *w.catalog->military, w.growth,
                  GrowthNavalHistoryContext{
                      *w.catalog->common, *w.catalog->family, w.family,
                      w.family_birth_enabled() ? &w.family_birth : nullptr}} {
    const auto &early = core.characters.early;
    take(validate_native_military_state_layout(w));
    take(validate_native_political_relations_layout(w));
    if (w.political_relations)
      relations.emplace(PoliticalRelationsReadContext{
          w.common, w.organization, *w.catalog->organization,
          *w.political_relations, *w.catalog->political_relations, w.military,
          *w.field_battle, *early.field_application_clock, *early.formations});
    if (w.political_takeover_enabled())
      territory = take(PoliticalTakeoverWorldAccess::create(
          w, *w.catalog->political_takeover, w.political_takeover, phase));
    const auto transport =
        take(selected_native_transport_versions(w.catalog->identity));
    if (transport.group) {
      take(validate_native_transport_layout(w));
      if (!w.movement_exact || !w.spatial_carriers ||
          !w.group_movement_executions)
        required("FieldOriginalGroupRootsRequired");
      auto selected_march = original_march_selection(w);
      const auto military_versions =
          take(selected_native_military_state_versions(w.catalog->identity));
      selected_march.selected_authority_time = military_versions.military_authority_time;
      if (selected_march.selected_authority_time) {
        if (selected_march.selected_authority_time != 1 ||
            military_versions.military_action_time != 1 ||
            !w.military_action_time || !w.action_elapsed || !body.scale ||
            early.military_access.military_authority_time_schema != 1)
          required("FieldOriginalMarchAuthoritySourcesRequired");
        march_authority.emplace(MilitaryAuthoritySourceContext{
            w.common, w.military, *w.military_action_time, *w.action_elapsed,
            ExactOfficeSourceReadContext{
                w.common, w.military, *w.military_action_time, *w.action_elapsed,
                *w.catalog->military, early.military_access, *body.scale,
                early.clock, *w.catalog, w.organization, w.family, body, {}, 2},
            selected_march.selected_authority_time, nullptr});
        take(validate_military_authority_source_context(*march_authority));
        selected_march.authority_sources = &*march_authority;
      }
      march.emplace(std::move(selected_march));
      contacts.emplace(FieldExactContactReadContext{
          w.common, w.military, *w.catalog->military, *w.field_battle,
          *w.catalog->travel, *w.movement_exact, *w.spatial_carriers,
          *w.group_movement_executions, body, *early.field_application_clock,
          *early.formations, *march});
      // 原经济 Opening reader 仅借期间身份；不因此选择 Training 玩法。
      contacts->periods = early.periods;
      // 同一 Complete owner 的原军事归属链；冷读仍由真实 Common cut 定位。
      contacts->original_military_access = &early.military_access;
      // 冷援军续行借同一个真实名册/期首 owner，寿命由本 Impl 顺序保护。
      contacts->original_rosters = &rosters;
      // 冷原suffix/参战读同样借已有只读资源，不等待热GroupContext。
      contacts->travel_services = &w.services;
      contacts->original_flags = w.catalog->immutable_interaction_flags.get();
      contacts->original_interaction = &w.interaction;
      // abilities值已构造；先把同Impl地址交给会复制context的placements，
      // 完成下方所有借口后才公开Scope，低层名册/身体不得反调完整几何。
      handoffs.emplace(FieldGroupHandoffReadContext{
          w.common, w.military, *w.field_battle, *w.catalog->travel,
          *w.catalog->field_battle, *w.movement_exact, *w.spatial_carriers,
          *w.group_movement_executions, *early.field_application_clock,
          &*contacts, relations ? &*relations : nullptr,
          territory ? &territory->territories() : nullptr, &body, body.deaths,
          body.life_sources, &abilities});
      placements.emplace(*handoffs);
      contacts->original_member_releases = &*placements;
    }
    abilities.learning = w.military_tactic_learning_enabled()
                             ? &w.military_tactic_learning
                             : nullptr;
    abilities.phase = phase;
    abilities.original_formations = early.formations;
    abilities.actual_clock = early.field_application_clock;
    abilities.original_life =
        w.family_life_enabled() && w.catalog->family_life->selected_version == 4
            ? &core.characters.family.life()
            : nullptr;
    abilities.original_rosters = &rosters;
    abilities.conditions = &body;
    abilities.permanent_attributes = &growth;
    abilities.attribute_absence = early.attribute_absence;
    abilities.active_membership = placements ? &*placements : nullptr;
    abilities.exact_contacts = contacts ? &*contacts : nullptr;
    abilities.diplomacy = relations ? &*relations : nullptr;
    abilities.territory = territory ? &territory->territories() : nullptr;
    abilities.original_read_boundary = &field_boundary;
    if (contacts) {
      finite_returns.emplace(*contacts, *world.catalog->field_battle, abilities);
      contacts->original_finite_returns = &*finite_returns;
    }

  }
};

NativeFieldWorldScope::NativeFieldWorldScope(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
NativeFieldWorldScope::~NativeFieldWorldScope() = default;
Status NativeFieldWorldScope::bind_original_named_custody(
    const NativeNamedCustodyAvailabilityRead &source) {
  return guarded<Done>([&] {
    const auto &w = impl_->world;
    const auto current = original_body(w, impl_->core);
    if (impl_->body.named_custody_availability ||
        !named_custody_availability_selected(impl_->body.catalog) ||
        current.named_custody_availability != &source ||
        !source.bound_to(w.common, w.military, *w.actor_foundation,
                         *w.catalog->actor_foundation) ||
        take(compare_world_moments(current.actual_now, impl_->body.actual_now)))
      required("CompleteNamedCustodyFieldBindingMismatch");
    impl_->body.named_custody_availability = &source;
    return Done{};
  });
}
Outcome<std::unique_ptr<NativeFieldWorldScope>>
NativeFieldWorldScope::create(const WorldBundle &world,
                              const CompleteExactWorldReadBindings &reads,
                              const DynamicStepInput *phase) {
  return guarded<std::unique_ptr<NativeFieldWorldScope>>([&] {
    if (!world.catalog)
      required("FieldOriginalCatalogRequired");
    take(validate_field_battle_layout(
        world.catalog->identity, world.catalog->field_battle.get(),
        world.field_battle ? &*world.field_battle : nullptr));
    if (!reads.original_core ||
        &reads.original_core->characters.early.world != &world ||
        &reads.original_core->characters.early.complete.reads() != &reads ||
        &reads.original_core->characters.family.body() !=
            &reads.character_conditions)
      required("FieldOriginalCompleteOwnerRequired");
    auto body = original_body(world, *reads.original_core);
    auto impl = std::unique_ptr<Impl>(
        new Impl(world, *reads.original_core, std::move(body), phase));
    return std::unique_ptr<NativeFieldWorldScope>(
        new NativeFieldWorldScope(std::move(impl)));
  });
}
// 新 Group 前序只借真实已构成 core；不能递归取得 Complete 发布面。
Outcome<std::unique_ptr<NativeFieldWorldScope>>
NativeFieldWorldScope::create_from_original_core(
    const WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    const DynamicStepInput *phase) {
  return guarded<std::unique_ptr<NativeFieldWorldScope>>([&] {
    if (!world.catalog || &core.characters.early.world != &world)
      required("FieldOriginalCompleteOwnerRequired");
    take(validate_field_battle_layout(
        world.catalog->identity, world.catalog->field_battle.get(),
        world.field_battle ? &*world.field_battle : nullptr));
    auto body = original_body(world, core);
    auto impl =
        std::unique_ptr<Impl>(new Impl(world, core, std::move(body), phase));
    return std::unique_ptr<NativeFieldWorldScope>(
        new NativeFieldWorldScope(std::move(impl)));
  });
}
Status NativeFieldWorldScope::bind_actual_group_context(
    const GroupMovementContext &group) {
  if (!impl_->contacts || impl_->contacts->group_context ||
      &group.travel_catalog != impl_->world.catalog->travel.get() ||
      &group.travel_services != &impl_->world.services ||
      &group.carriers != &impl_->core.characters.spatial.original ||
      !impl_->world.catalog->immutable_interaction_flags)
    return common_failure("FieldOriginalGroupBindingMismatch");
  impl_->contacts->group_context = &group;
  impl_->contacts->travel_services = &impl_->world.services;
  impl_->contacts->original_flags =
      impl_->world.catalog->immutable_interaction_flags.get();
  impl_->contacts->original_interaction = &impl_->world.interaction;
  return common_success();
}
const CharacterConditionSources &
NativeFieldWorldScope::body_sources() const noexcept {
  return impl_->body;
}
const MilitaryMarchAuthorSelection *
NativeFieldWorldScope::march_selection() const noexcept {
  return impl_->march ? &*impl_->march : nullptr;
}
const GroupContactMemberReleaseRead *
NativeFieldWorldScope::member_releases() const noexcept {
  return impl_->placements ? &*impl_->placements : nullptr;
}
bool NativeFieldWorldScope::bound_to(const WorldBundle &world) const noexcept {
  return &impl_->world == &world;
}
const FieldBattleAbilityReadContext &
NativeFieldWorldScope::abilities() const noexcept {
  return impl_->abilities;
}
const FieldExactContactReadContext *
NativeFieldWorldScope::exact_contacts() const noexcept {
  return impl_->contacts ? &*impl_->contacts : nullptr;
}
const PoliticalRelationsReadContext *
NativeFieldWorldScope::diplomacy() const noexcept {
  return impl_->relations ? &*impl_->relations : nullptr;
}
const FieldGroupHandoffReadContext *
NativeFieldWorldScope::group_handoffs() const noexcept {
  return impl_->handoffs ? &*impl_->handoffs : nullptr;
}
const FieldBattleExitPlacementRead *
NativeFieldWorldScope::placements() const noexcept {
  return impl_->placements ? &*impl_->placements : nullptr;
}
const MilitaryFiniteReturnSourceRead *
NativeFieldWorldScope::finite_returns() const noexcept {
  return impl_->finite_returns ? &*impl_->finite_returns : nullptr;
}
Status NativeFieldWorldScope::validate() const {
  const auto &w = impl_->world;
  auto physical = validate_native_movement_boundary_continuation_layout(w);
  if (!physical.ok) return physical;
  auto valid = validate_field_battle_world(
      w.common, w.military, *w.catalog->travel, *w.catalog->field_battle,
      *w.field_battle, &impl_->abilities);
  if (!valid.ok)
    return valid;
  auto continuation = selected_native_movement_boundary_continuation_version(w.catalog->identity);
  if (!continuation.ok) return {false, {}, std::move(continuation.error)};
  if (continuation.value) {
    if (!impl_->contacts || !impl_->relations)
      return common_failure("MovementBoundaryContinuationOriginalWorldSourcesRequired");
    valid = validate_movement_boundary_continuation_sources(
        *impl_->contacts, *impl_->relations, *w.catalog->field_battle);
    if (!valid.ok) return valid;
  }
  valid = validate_field_battle_command_results(w.common, &*w.field_battle,
                                               impl_->abilities.original_life);
  if (!valid.ok) return valid;
  if (!impl_->contacts)
    return reject_unselected_army_exit_sources(w);
  // 仅完整候选封口调用：选择、真实步进、ready、取消/续行和损失后果全链反覆盖。
  if (!impl_->handoffs || !impl_->finite_returns)
    return common_failure("FieldArmyExitLossOriginalHandoffsRequired");
  return validate_field_army_exit_sources(*impl_->contacts,
      *w.catalog->field_battle, impl_->abilities, *impl_->handoffs);
}
Status validate_native_field_world_sources(const WorldBundle &world) {
  auto result = guarded<Done>([&] {
    if (!world.catalog)
      required("FieldOriginalCatalogRequired");
    take(validate_field_battle_layout(
        world.catalog->identity, world.catalog->field_battle.get(),
        world.field_battle ? &*world.field_battle : nullptr));
    // 缺原Field或空世界早返之前也反扫续行孤头/未选表，不让坏冷档藏在空分支。
    take(validate_native_movement_boundary_continuation_layout(world));
    // 在所有 empty/legacy 早返之前检查，不让无 Group 的自动选择绕过领域源门。
    if (!world.catalog->field_group_passage)
      take(reject_unselected_army_exit_sources(world));
    if (!world.field_battle) {
      take(validate_field_battle_command_results(world.common, nullptr));
      return Done{};
    }
    if (world.catalog->field_group_passage) {
      auto phase = own_field_source_validation_phase(world);
      auto complete = take(world.complete_exact_time_bindings());
      if (!complete || !complete->reads().original_core)
        required("FieldOriginalCompleteOwnerRequired");
      auto scope = take(NativeFieldGroupWorld::create(
          world, *complete->reads().original_core));
      take(scope->validate());
      return Done{};
    }
    const auto &field = *world.field_battle;
    // 没有任何 Field 行时仍调用原 full 入口反扫孤头；不是跳过业务校验。
    const bool empty =
        field.sessions.empty() && field.contacts.empty() &&
        field.boundary_contacts.empty() && field.army_bindings.empty() &&
        field.exits.empty() && field.council_facts.empty() &&
        field.army_exit_plans.empty() &&
        (!world.movement_exact ||
         (world.movement_exact->boundary_contacts.empty() &&
          world.movement_exact->boundary_continuations.empty())) &&
        (!world.group_movement_executions ||
         (world.group_movement_executions->contact_member_releases.empty() &&
          world.group_movement_executions->member_loss_handoffs.empty()));
    if (empty || !world.catalog->exact_time ||
        !exact_complete_selected(*world.catalog->exact_time)) {
      take(validate_field_battle_world(world.common, world.military,
                                       *world.catalog->travel,
                                       *world.catalog->field_battle, field));
      take(validate_field_battle_command_results(world.common, &field));
      return Done{};
    }
    auto phase = own_field_source_validation_phase(world);
    auto complete = take(world.complete_exact_time_bindings());
    if (!complete)
      required("FieldOriginalCompleteOwnerRequired");
    auto scope = take(NativeFieldWorldScope::create(world, complete->reads()));
    take(scope->validate());
    return Done{};
  });
  return {result.ok, {}, std::move(result.error)};
}
} // namespace hanmo::resident
