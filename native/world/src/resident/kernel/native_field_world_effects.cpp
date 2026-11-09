#include "native_field_world_effects.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_domain_read_versions.hpp"
#include "native_field_world_scope.hpp"
#include "native_life_age_world.hpp"
#include "resident/domains/actor_foundation/field_injury_write_v1.hpp"
#include "resident/domains/actor_foundation/injury_progress_replay_v1.hpp"
#include "resident/domains/actor_foundation/life_condition.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_army_exit_ordinary_loss.hpp"
#include "resident/domains/military/field_battle/field_death_consumer.hpp"
#include "resident/domains/military/field_battle/field_member_loss_bound_apply.hpp"
#include "resident/domains/military/field_battle/field_member_loss_desertion.hpp"
#include "resident/domains/military/named_military_custody_terms.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <new>
#include <stdexcept>

namespace hanmo::resident {
namespace {
// 私有错误边界只保原失败；调用者必须丢弃整候选而非发布已落的半组后果。
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
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code).error};
}
// 读图由本次实际 Complete owner 构造；每次借用都核同世界与原活跃捕获。
template <class F>
Status with_original_sources(WorldBundle &world,
                             CompleteExactWorldBindingOwner &owner,
                             const SourceRef &segment,
                             ExactPhaseWriteContext &writes, F &&consume) {
  try {
    require(world.active_exact_write_context() == &writes &&
                world.common.actor_condition_capture_identity() &&
                world.common.actor_location_capture_identity() &&
                world.has_active_read_phase() && world.read_stamps &&
                world.actor_foundation && world.character_injury_sources &&
                world.growth_permanent && world.field_battle &&
                world.movement_exact && world.group_movement_executions,
            "FieldEffectsOriginalCandidateRequired");
    const auto *core = owner.reads().original_core;
    require(core && &core->characters.early.world == &world &&
                &core->characters.early.complete == &owner &&
                core->characters.early.field_application_clock,
            "FieldEffectsOriginalCompleteOwnerRequired");
    auto scope = take(NativeFieldWorldScope::create(world, owner.reads()));
    const auto *body = scope->abilities().conditions;
    const auto &clock = *core->characters.early.field_application_clock;
    require(
        scope->bound_to(world) && body && &body->common == &world.common &&
            &body->state == &*world.actor_foundation &&
            body->injury_sources == &*world.character_injury_sources &&
            body->injury_field == &*world.field_battle &&
            body->military == &world.military &&
            clock.bound_to(world.common, world.military, *world.field_battle),
        "FieldEffectsOriginalSourceMismatch");
    const auto actual = take(read_field_segment_settlement_source(
        world.common, world.military, *world.field_battle, segment, &clock));
    require(
        actual.casualties && actual.settlement &&
            take(compare_world_moments(actual.settlement->applied_at,
                                       take(clock.read_current_moment()))) == 0,
        "FieldEffectsActualSegmentRequired");
    // 每次真实后果采用与异常退出都使旧读票失效，不依赖外层隐式包裹。
    ReadStampWriteBoundary source_boundary(*world.read_stamps);
    consume(*scope, *body, clock, core->characters.early, actual);
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldEffectsResourceExhausted", {},
                          ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldEffectsResourceExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace

Status apply_native_field_segment_injuries_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    const SourceRef &segment, ExactPhaseWriteContext &writes) {
  return with_original_sources(
      world, owner, segment, writes,
      [&](const NativeFieldWorldScope &, const CharacterConditionSources &body,
          const FieldSegmentApplicationRead &clock,
          const NativeCompleteWorldEarlySources &,
          const FieldSegmentSettlementSourceRead &actual) {
        auto *capability = owner.growth_capability_writer();
        const CharacterInjuryReplaySourcesV1 injury{
            body, *world.character_injury_sources, body.injury_personal,
            &*world.field_battle};
        const SourceRef casualty{"military", "military_casualties",
                                 actual.casualties->id};
        for (const auto &change : actual.casualties->actor_changes) {
          if (change.status != "wounded")
            continue;
          require(capability != nullptr,
                  "FieldEffectsActualCapabilityWriterRequired");
          // 此原写者已即时记录能力修订；这里只收真实 Bound，不再二写身体。
          writes.condition_writes.push_back(
              take(apply_field_segment_injury_in_candidate_v1(
                  world.common, *world.actor_foundation,
                  *world.character_injury_sources, injury, casualty,
                  change.actor_id, clock, *world.growth_permanent,
                  *capability)));
        }
      });
}

Status apply_native_field_segment_captivity_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    const SourceRef &segment, ExactPhaseWriteContext &writes) {
  return with_original_sources(
      world, owner, segment, writes,
      [&](const NativeFieldWorldScope &scope,
          const CharacterConditionSources &body,
          const FieldSegmentApplicationRead &clock,
          const NativeCompleteWorldEarlySources &early,
          const FieldSegmentSettlementSourceRead &actual) {
        const auto *handoffs = scope.group_handoffs();
        require(handoffs != nullptr, "FieldEffectsOriginalGroupRequired");
        const NamedMilitaryCustodyContext custody{
            world.common,
            world.military,
            *world.catalog->military,
            world.catalog->identity,
            &*world.field_battle,
            world.catalog->naval ? &world.naval : nullptr,
            early.formations,
            &early.clock,
            &early.military_access,
            &clock};
        take(record_named_military_custody_terms_in_candidate(
            world.common, world.military, custody, *actual.casualties));
        NativeDomainReadVersionSource versions(world, early.formations,
                                               early.returns, early.periods);
        auto current = world.make_current_read_stamp_reader(
            versions, {4096, 4096, 1024 * 1024});
        const SourceRef casualty{"military", "military_casualties",
                                 actual.casualties->id};
        for (const auto &change : actual.casualties->actor_changes) {
          if (change.status != "captured")
            continue;
          ReadStampWriteBoundary boundary(*world.read_stamps);
          auto stamp = take(current->capture(
              {{ResourceKey::Kind::actor, change.actor_id}},
              {{"actor_foundation", "condition_head", change.actor_id}},
              {casualty.id}));
          require(bool(stamp), "FieldEffectsActualReadStampRequired");
          auto applied = take(apply_field_captured_member_in_candidate(
              world.common, *world.actor_foundation, body, world.military,
              *world.field_battle, casualty, change.actor_id, clock, *stamp,
              *world.read_stamps, versions));
          auto proof = take(read_actual_field_captured_member_application(
              world.common, *world.actor_foundation, body, world.military,
              *world.field_battle, clock, applied));
          require(proof.availability != nullptr,
                  "FieldEffectsActualAvailabilityRequired");
          // proof 借用在 S1 采用后才结束；复制的是原写者签发的真实捕获见证。
          writes.spatial_location_writes.push_back(
              take(apply_field_captured_group_member_with_capture_in_candidate(
                  world.common, *world.movement_exact,
                  *world.group_movement_executions, *handoffs, applied,
                  world.checkpoint_copy_memory)));
          writes.availability_writes.push_back(*proof.availability);
        }
      });
}

Status apply_native_field_segment_deaths_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    const SourceRef &segment, ExactPhaseWriteContext &writes) {
  return with_original_sources(
      world, owner, segment, writes,
      [&](const NativeFieldWorldScope &scope,
          const CharacterConditionSources &body,
          const FieldSegmentApplicationRead &clock,
          const NativeCompleteWorldEarlySources &early,
          const FieldSegmentSettlementSourceRead &actual) {
        const auto *handoffs = scope.group_handoffs();
        require(handoffs && body.life_sources &&
                    body.deaths == &world.family_life,
                "FieldEffectsOriginalLifeAndGroupRequired");
        NativeDomainReadVersionSource versions(world, early.formations,
                                               early.returns, early.periods);
        auto current = world.make_current_read_stamp_reader(
            versions, {4096, 4096, 1024 * 1024});
        auto applied = take(apply_field_segment_deaths_in_candidate(
            world.common, *world.actor_foundation, world.family_life,
            *world.catalog->actor_foundation, world.military,
            *world.field_battle, *body.life_sources, segment, clock, *current,
            *world.read_stamps, versions, &*world.growth_permanent,
            owner.growth_capability_writer(), owner.care_life_writer(), &body));
        const SourceRef casualty{"military", "military_casualties",
                                 actual.casualties->id};
        const auto life_factory = native_life_age_world_factory();
        require(life_factory.after_actual_death != nullptr,
                "FieldEffectsOriginalDeathContinuationRequired");
        for (auto &life : applied) {
          const auto *death = world.family_life.deaths.find(life.death_ref.id);
          require(death && !life.already_applied && !life.actual_writes.empty(),
                  "FieldEffectsActualDeathApplicationRequired");
          // S1 必须先看见完整 LifeConditionApplication；不能先 move 掉其证明。
          writes.spatial_location_writes.push_back(
              take(apply_field_dead_group_member_with_capture_in_candidate(
                  world.common, *world.movement_exact,
                  *world.group_movement_executions, *handoffs, casualty,
                  death->actor_id, life, world.checkpoint_copy_memory)));
          for (auto &write : life.actual_writes)
            writes.condition_writes.push_back(std::move(write));
          take(life_factory.after_actual_death(world, life.death_ref));
        }
      });
}
// 逃散与被俘独立采用；真退出源及原 Capacity 捕获全由同候选写者核验。
Status apply_native_field_segment_desertions_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    const SourceRef &segment, ExactPhaseWriteContext &writes) {
  return with_original_sources(
      world, owner, segment, writes,
      [&](const NativeFieldWorldScope &scope, const CharacterConditionSources &,
          const FieldSegmentApplicationRead &,
          const NativeCompleteWorldEarlySources &early,
          const FieldSegmentSettlementSourceRead &actual) {
        const auto *handoffs = scope.group_handoffs();
        require(handoffs != nullptr, "FieldEffectsOriginalGroupRequired");
        NativeDomainReadVersionSource versions(world, early.formations,
                                               early.returns, early.periods);
        auto current = world.make_current_read_stamp_reader(
            versions, {4096, 4096, 1024 * 1024});
        const SourceRef casualty{"military", "military_casualties",
                                 actual.casualties->id};
        for (const auto &change : actual.casualties->actor_changes) {
          if (change.status != "deserted")
            continue;
          ReadStampWriteBoundary boundary(*world.read_stamps);
          auto stamp = take(current->capture(
              {{ResourceKey::Kind::actor, change.actor_id}},
              {{"actor_foundation", "condition_head", change.actor_id}},
              {casualty.id}));
          require(bool(stamp), "FieldEffectsActualReadStampRequired");
          auto applied =
              take(apply_field_deserted_group_member_with_capture_in_candidate(
                  world.common, *world.actor_foundation, *world.movement_exact,
                  *world.group_movement_executions, *handoffs, casualty,
                  change.actor_id, *stamp, *world.read_stamps, versions,
                  world.checkpoint_copy_memory));
          writes.availability_writes.push_back(std::move(applied.availability));
          writes.spatial_location_writes.push_back(std::move(applied.location));
        }
      });
}
// 截止步本身是原父源；先核真实几何，再记整普通兵名册，只有真Common人物走S4/S1。
Status apply_native_field_army_exit_ordinary_losses_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    const SourceRef &step, ExactPhaseWriteContext &writes) {
  try {
    require(world.active_exact_write_context() == &writes &&
                world.common.actor_condition_capture_identity() &&
                world.common.actor_location_capture_identity() &&
                world.has_active_read_phase() && world.read_stamps &&
                world.actor_foundation && world.field_battle &&
                world.movement_exact && world.group_movement_executions,
            "FieldArmyExitEffectsOriginalCandidateRequired");
    const auto *core = owner.reads().original_core;
    require(core && &core->characters.early.world == &world &&
                &core->characters.early.complete == &owner &&
                core->characters.early.field_application_clock,
            "FieldEffectsOriginalCompleteOwnerRequired");
    auto scope = take(NativeFieldWorldScope::create(world, owner.reads()));
    const auto *contacts = scope->exact_contacts();
    const auto *handoffs = scope->group_handoffs();
    const auto *body = scope->abilities().conditions;
    const auto &early = core->characters.early;
    const auto &clock = *early.field_application_clock;
    require(scope->bound_to(world) && contacts && handoffs && body &&
                &body->common == &world.common &&
                &body->state == &*world.actor_foundation &&
                handoffs->original_body == body &&
                handoffs->original_abilities == &scope->abilities() &&
                clock.bound_to(world.common, world.military, *world.field_battle),
            "FieldArmyExitEffectsOriginalSourceMismatch");
    NativeDomainReadVersionSource versions(world, early.formations,
                                           early.returns, early.periods);
    auto current = world.make_current_read_stamp_reader(
        versions, {4096, 4096, 1024 * 1024});
    require(bool(current), "FieldEffectsActualReadStampRequired");
    ReadStampWriteBoundary source_boundary(*world.read_stamps);
    // 域batch封住先验与写入之间的窗口：全量来源先验，全部S4齐后才full S1绑定。
    // 真step无ordinary离场时返回nullopt及空effects，不创建空伤亡事实。
    auto applied = take(apply_field_army_exit_ordinary_losses_with_capture_in_candidate(
        world.common, world.military, *world.actor_foundation, *world.movement_exact,
        *world.group_movement_executions, *handoffs, step, *current,
        *world.read_stamps, versions, world.checkpoint_copy_memory));
    for (auto &write : applied.availability)
      writes.availability_writes.push_back(std::move(write));
    for (auto &write : applied.locations)
      writes.spatial_location_writes.push_back(std::move(write));
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldEffectsResourceExhausted", {}, ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldEffectsResourceExhausted", {}, ErrorKind::internal);
  }
}
} // namespace hanmo::resident
