#include "native_local_move_world.hpp"
#include "native_local_move_catalog.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/travel/local_move_source.hpp"
#include "resident/domains/travel/travel.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "read_stamp.hpp"
#include <new>
#include <stdexcept>

namespace hanmo::resident {
namespace {
// 原候选失败保持首错并交外层撤销；不在此修复或提交部分状态。
struct Failure { RuleError error; };
template <class T> T take(Outcome<T> result) {
  if (!result.ok) throw Failure{std::move(result.error)};
  return std::move(result.value);
}
void take(Status result) {
  if (!result.ok) throw Failure{std::move(result.error)};
}
void require(bool value, const char *code) {
  if (!value) throw Failure{common_failure(code).error};
}
template <class F> Status guarded(F &&body) {
  try { body(); return common_success(); }
  catch (Failure &failure) { return {false, {}, std::move(failure.error)}; }
  catch (const std::bad_alloc &) {
    return common_failure("LocalMoveResourceExhausted", {}, ErrorKind::internal);
  }
  catch (const std::length_error &) {
    return common_failure("LocalMoveResourceExhausted", {}, ErrorKind::internal);
  }
}
// 只自持缺少的本次读票；已有原阶段票只借用，不能替换或提前退出。
std::unique_ptr<PhaseCurrentReadScope> own_read_phase(const WorldBundle &world,
                                                   const WorldMoment &at) {
  require(bool(world.read_stamps), "LocalMoveOriginalReadStampRequired");
  if (world.has_active_read_phase()) return {};
  return take(world.enter_phase_read_context(
      "local_move_original_sources", "travel", {at, at}, at));
}
// 各被借根必须与正在读取的 World 完全同一，不能从另一个候选的副本拼出前驱。
const GrowthFixedSpatialSources &original_spatial(
    const WorldBundle &world, const CompleteExactWorldReadBindings &actual,
    const WorldMoment &at) {
  require(world.catalog && world.catalog->common && world.catalog->actor_foundation &&
              world.catalog->travel && world.actor_foundation && world.movement_exact &&
              world.spatial_carriers && world.transport_registrations && actual.original_core,
          "LocalMoveOriginalWorldSourcesRequired");
  const auto &core = *actual.original_core;
  const auto &spatial = core.characters.spatial;
  require(&core.characters.early.world == &world &&
              &actual.movement == &*world.movement_exact &&
              &spatial.movement == &*world.movement_exact &&
              &spatial.carriers == &*world.spatial_carriers &&
              &spatial.registrations == &*world.transport_registrations,
          "LocalMoveOriginalWorldSourcesMismatch");
  require(take(compare_world_moments(take(core.characters.early.clock.read_current_moment()), at)) == 0 &&
              take(compare_world_moments(take(spatial.original.current_moment()), at)) == 0,
          "LocalMoveOriginalWorldMomentMismatch");
  return spatial;
}
// 此值只拥有不可变原前驱快照；任何 read phase、scope、owner 都不跨后面的 writer。
struct OriginalMoveInput { WorldMoment at; ActorSpatialLocationView before; };
OriginalMoveInput read_original_input(
    const WorldBundle &world, const StableId &actor,
    const CompleteExactWorldReadBindings *borrowed) {
  const auto at = take(world.actual_phase_moment());
  auto phase = own_read_phase(world, at);
  std::unique_ptr<CompleteExactWorldBindingOwner> owner;
  if (!borrowed) {
    owner = take(world.complete_exact_time_bindings());
    require(bool(owner), "LocalMoveOriginalCompleteOwnerRequired");
    borrowed = &owner->reads();
  }
  const auto &spatial = original_spatial(world, *borrowed, at);
  take(validate_growth_fixed_spatial_source_moment(spatial, at));
  const auto current = take(read_current_common_fact_prefix(world.common));
  // 当前前缀只借 Common 头；历史读口使用本次拥有的同一 count/末头身份，不留裸指针。
  ActorSpatialActualFactPrefix prefix{current.common_fact_count, {}};
  if (current.last_common_fact)
    prefix.last_common_fact.emplace(SourceRef{
        current.last_common_fact->domain, current.last_common_fact->kind,
        current.last_common_fact->id});
  auto before = take(read_actor_spatial_location_at_actual_fact_prefix(
      world.common, *world.catalog->common, spatial.movement, spatial.carriers,
      spatial.registrations, *world.actor_foundation, *world.catalog->actor_foundation,
      spatial.original, actor, at, prefix, spatial.military_handoffs, {},
      world.catalog->family.get(), spatial.births, spatial.military_actions,
      spatial.custody_releases, spatial.field_exits));
  return {at, std::move(before)};
}
} // namespace

Status apply_native_local_move_in_candidate(
    WorldBundle &world, const StableId &actor, const StableId &place,
    const CompleteExactWorldReadBindings *actual) {
  return guarded([&] {
    require(world.catalog && world.catalog->travel, "LocalMoveOriginalCatalogRequired");
    const auto selected = take(selected_native_local_move_version(world.catalog->identity));
    if (!selected) {
      take(apply_move_local_location(world.common, *world.catalog->travel, actor, place));
      return;
    }
    auto *writes = world.active_exact_write_context();
    require(writes && world.movement_exact && world.common.actor_location_capture_identity(),
            "LocalMoveActualCaptureRequired");
    const auto original = read_original_input(world, actor, actual);
    // 前驱读者与自持票已销毁；只保真实值。原位置 writer 恰调用一次，同值写也记 tap。
    auto bound = take(apply_local_move_with_source_in_candidate(
        world.common, *world.movement_exact, *world.catalog->travel, actor, place,
        original.at, original.before));
    writes->spatial_location_writes.push_back(std::move(bound));
    // 原 dispatcher 随后执行 after_local_move 和唯一原 write boundary；不额外增加版本边界。
    // 原 settle 统一消费这一次真实 spatial 写。
  });
}

Status validate_native_local_move_world_sources(const WorldBundle &world) {
  return guarded([&] {
    take(validate_native_local_move_layout(world));
    const auto selected = take(selected_native_local_move_version(world.catalog->identity));
    // layout 已反扫所有实际 anchor/孤头/残留，空历史不构造无关 domain owner。
    if (!selected || world.movement_exact->local_moves.rows().empty()) return;
    const auto at = take(world.actual_phase_moment());
    auto phase = own_read_phase(world, at);
    auto owner = take(world.complete_exact_time_bindings());
    require(bool(owner), "LocalMoveOriginalCompleteOwnerRequired");
    const auto &actual = owner->reads();
    const auto &spatial = original_spatial(world, actual, at);
    take(validate_growth_fixed_spatial_source_moment(spatial, at));
    ActorSpatialHeadSourceReadContext context{
        world.common, *world.catalog->common, spatial.movement, spatial.carriers,
        spatial.registrations, *world.actor_foundation, *world.catalog->actor_foundation,
        spatial.original, world.catalog->family.get(), spatial.births, {},
        spatial.military_actions};
    take(validate_local_move_source_history(
        context, spatial.military_handoffs, spatial.custody_releases, spatial.field_exits));
  });
}
} // namespace hanmo::resident
