#include "native_growth_market_sources.hpp"
// 完整正片、原owner
// due与宏观closing共用同一候选。所有失败由原事务丢弃，不发布半个时刻。
#include "command_types.hpp"
#include "complete_growth_command_binding.hpp"
#include "resident/domains/growth/growth_pending_started.hpp"
#include "resident/domains/growth/growth_quiet_source.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/storage/common_capture_read_set.hpp"
#include "resident/storage/common_command_publication.hpp"
#include <array>
#include "common_command_sources.hpp"
#include "exact_complete_world.hpp"
#include "native_director_wake_world.hpp"
#include "native_domain_read_versions.hpp"
#include "native_field_military_schedule.hpp"
#include "native_field_boundary_continuation.hpp"
#include "native_field_window_driver.hpp"
#include "native_field_winner_opening.hpp"
#include "native_growth_validation.hpp"
#include "native_interaction_catalog.hpp"
#include "native_interaction_world.hpp"
#include "native_military_training_period.hpp"
#include "native_original_social_world.hpp"
#include "native_task_execution_state.hpp"
#include "private_shop_integration.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/growth/growth_condition_input_consumer.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/interaction/interaction_action_confirmation.hpp"
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include "resident/domains/local_economy/private_shop_household_custody.hpp"
#include "resident/domains/local_economy/private_shop_market_commands.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/domains/military/military_calendar_phase.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_return_source.hpp"
#include "resident/domains/tasks/task_exact_life.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
Status resume_complete_private_shop_dialogue_in_candidate(WorldBundle &,
                                                          const StableId &);

// The original advance/Opening stack owns this pointer. Clones start unbound;
// the scope cannot survive the actual capture or become saved world state.
class WorldExactWriteScope : public CheckpointAllocatedObject {
  WorldBundle &world_;

public:
  WorldExactWriteScope(WorldBundle &world, ExactPhaseWriteContext &writes)
      : world_(world) {
    world_.active_exact_writes_ = &writes;
  }
  ~WorldExactWriteScope() { world_.active_exact_writes_ = nullptr; }
  WorldExactWriteScope(const WorldExactWriteScope &) = delete;
  WorldExactWriteScope &operator=(const WorldExactWriteScope &) = delete;
};
// Only the actual advance below can bind this non-persistent witness. The
// pointed-to span was selected by the original S0 graph and outlives the scope.
class WorldExactAdvanceSpanScope final {
  ExactPhaseWriteContext &writes_;

public:
  WorldExactAdvanceSpanScope(ExactPhaseWriteContext &writes,
                             const MomentSpan &span)
      : writes_(writes) {
    writes_.actual_advance_span_ = &span;
  }
  ~WorldExactAdvanceSpanScope() { writes_.actual_advance_span_ = nullptr; }
  WorldExactAdvanceSpanScope(const WorldExactAdvanceSpanScope &) = delete;
  WorldExactAdvanceSpanScope &
  operator=(const WorldExactAdvanceSpanScope &) = delete;
};
namespace {
struct Failure {
  RuleError error;
};
void fail(const char *code) {
  throw Failure{common_failure(code, "complete_exact_time").error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
template <class T, class F> Outcome<T> guard(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("CompleteExactResourceExhausted",
                           "complete_exact_time", ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("CompleteExactResourceExhausted",
                           "complete_exact_time", ErrorKind::internal)
                .error};
  }
}
bool same(const WorldMoment &a, const WorldMoment &z, ExactArithmeticBudget b) {
  return take(compare_world_moments(a, z, b)) == 0;
}
WorldMoment initial_at(const WorldBundle &world) {
  if (!world.catalog->actor_foundation ||
      world.catalog->actor_foundation->actors.empty())
    fail("MissingExactInitialOrigin");
  const auto tick = world.catalog->actor_foundation->actors.rows()
                        .front()
                        .second.scenario_origin_ref.initial_tick;
  for (const auto &actor : world.catalog->actor_foundation->actors.rows())
    if (actor.second.scenario_origin_ref.initial_tick != tick)
      fail("InconsistentExactInitialOrigin");
  return WorldMoment{tick, {}};
}
void required(const WorldBundle &world) {
  if (!world.catalog->exact_time ||
      !exact_complete_selected(*world.catalog->exact_time) ||
      !world.world_timeline || !world.action_elapsed ||
      !world.actor_foundation || !world.facility_claims)
    fail("CompleteExactStatePairRequired");
}
ExactPeriodContext period_at(const WorldBundle &world,
                             const WorldMoment &current, bool ready) {
  const auto tick = current.half_day_tick;
  if ((ready && tick == std::numeric_limits<Integer>::min()) ||
      (!ready && tick == std::numeric_limits<Integer>::max()))
    fail("ExactPeriodBoundaryOverflow");
  return {world.world_timeline->period_id,
          WorldMoment{ready ? tick - 1 : tick, {}}, ready ? tick : tick + 1};
}
void begin_writes(WorldBundle &world) {
  if (world.active_exact_write_context())
    fail("ExactWriteCaptureScopeMismatch");
  world.note_write_boundary();
  take(world.common.begin_actor_condition_capture());
  take(world.common.begin_actor_location_capture());
}
void merge_growth_input_effects(WorldBundle &world,
                                ExactPhaseWriteContext &writes,
                                GrowthConditionInputEffects effects) {
  for (auto &proof : effects.pending_study_releases) {
    if (!proof.action_id())
      fail("StudyPendingReleaseProofRequired");
    for (const auto &prior : writes.pending_study_releases)
      if (prior.action_id() && *prior.action_id() == *proof.action_id())
        fail("DuplicateStudyPendingReleaseProof");
    writes.pending_study_releases.push_back(std::move(proof));
  }
  take(validate_pending_growth_releases(world, writes.pending_study_releases));
  for (auto &actor : effects.suspended_actors)
    if (std::find(writes.advance.suspended_actors.begin(),
                  writes.advance.suspended_actors.end(),
                  actor) == writes.advance.suspended_actors.end())
      writes.advance.suspended_actors.push_back(std::move(actor));
  for (auto &row : effects.completed_actions) {
    ActionCompletion completion;
    completion.growth = std::move(row.second);
    if (!writes.advance.completed_actions.insert(row.first,
                                                 std::move(completion)))
      fail("DuplicateGrowthExactCompletion");
  }
}
void adopt_interaction_condition_deaths(
    WorldBundle &world, ExactPhaseWriteContext &writes,
    const CompleteExactWorldReadBindings &access,
    const CharacterConditionSources &body, const WorldMoment &now,
    ExactArithmeticBudget b) {
  if (writes.interaction_condition_cursor > writes.condition_writes.size() ||
      writes.interaction_condition_consumer_active ||
      world.active_exact_write_context() != &writes ||
      !world.common.actor_condition_capture_identity())
    throw Failure{common_failure("InteractionDeathCapturePrefixMismatch", {},
                                 ErrorKind::internal)
                      .error};
  const auto selected =
      take(selected_native_interaction_versions(world.catalog->identity));
  const auto task_execution =
      take(selected_native_task_execution_version(world.catalog->identity));
  if (!selected.exact_sources() && task_execution != 2) {
    writes.interaction_condition_cursor = writes.condition_writes.size();
    return;
  }
  const auto *group = selected.exact_sources()
                          ? take(read_native_interaction_world(world, access))
                          : nullptr;
  if (!body.life_sources || body.deaths != &world.family_life ||
      &body.common != &world.common ||
      &body.state != &*world.actor_foundation || !same(body.actual_now, now, b))
    throw Failure{common_failure("InteractionDeathOriginalBodyRequired", {},
                                 ErrorKind::internal)
                      .error};
  struct Active {
    bool &value;
    explicit Active(bool &v) : value(v) { value = true; }
    ~Active() { value = false; }
  } active{writes.interaction_condition_consumer_active};
  while (writes.interaction_condition_cursor < writes.condition_writes.size()) {
    const auto *receipt = take(read_actual_actor_condition_write(
        world.common, *world.actor_foundation,
        writes.condition_writes[writes.interaction_condition_cursor]));
    if (receipt->life_transition_ref) {
      const DeathRef ref{"family_life", "family_death",
                         *receipt->life_transition_ref};
      const auto death = take(read_verified_death4(
          world.common, world.family_life, *body.life_sources, ref, now));
      if (death.actor_id != receipt->actor_id ||
          receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
          death.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
          death.recorded_at.kind != ActorFoundationAt::Kind::exact_moment ||
          !same(receipt->effective_at.exact_moment, now, b) ||
          !same(death.effective_at.exact_moment, now, b) ||
          !same(death.recorded_at.exact_moment, now, b))
        throw Failure{common_failure("InteractionDeathOriginalWriteCutMismatch",
                                     receipt->actor_id, ErrorKind::internal)
                          .error};
      // Task's original participant stop is independent of S3 selection.
      // It adopts its real terminal and releases the actual Action before
      // an optional SessionLife consumer can accept that source.
      if (task_execution == 2) {
        take(consume_original_task_death_in_candidate(
            world, access.character_conditions, ref));
        world.note_write_boundary();
      }
      if (group)
        take(group->apply_actual_death_in_candidate(world, ref, writes));
    }
    // Only a successfully consumed real captured prefix advances this
    // candidate-local cursor. A failed candidate is never published/reused.
    ++writes.interaction_condition_cursor;
  }
}
void adopt_growth_condition_inputs(WorldBundle &world,
                                   ExactPhaseWriteContext &writes,
                                   ExactArithmeticBudget b,
                                   const GrowthPendingStartedProof* pending=nullptr) {
  if (writes.condition_writes.empty() && writes.availability_writes.empty())
    return;
  const auto now = take(world.actual_phase_moment());
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase())
    phase = take(world.enter_phase_read_context("growth_actual_input_adoption",
                                                "growth", {now, now}, now));
  auto owner = take(world.complete_exact_time_bindings());
  if (!owner)
    fail("CompleteGrowthOwningLeaseRequired");
  auto body = take(owner->reads().character_conditions.sources());
  if (&body.common != &world.common || body.growth != &world.growth ||
      !world.action_elapsed || body.elapsed != &*world.action_elapsed ||
      !same(body.actual_now, now, b))
    fail("CompleteGrowthCandidateBindingMismatch");
  NativeDomainReadVersionSource versions(
      world, owner->reads().military_formations,
      owner->reads().military_returns, owner->reads().military_supply_periods);
  auto reads =
      world.make_current_read_stamp_reader(versions, {4096, 4096, 1024 * 1024});
  auto original = world.services.growth_access();
  auto market = take(read_native_growth_market_exact_sources(world));
  GrowthFixedCaptureServices services{
      *reads,
      *world.read_stamps,
      versions,
      original,
      owner->growth_start_participation_writer(),
      market ? &*market : nullptr};
  auto effects = take(consume_growth_condition_inputs_in_candidate(
      world.common, world.growth, body, *world.action_elapsed, services,
      writes.condition_writes, writes.availability_writes,
      [&world](const StableId &action) {
        return release_action(world, action);
      },pending));
  merge_growth_input_effects(world, writes, std::move(effects));
  adopt_interaction_condition_deaths(world, writes, owner->reads(), body, now,
                                     b);
}
void validate_interaction_death_capture(WorldBundle &world,
                                        ExactPhaseWriteContext &writes) {
  if (!writes.pending_interaction_deaths ||
      !writes.pending_interaction_deaths->has_pending())
    return;
  // Never run full original Life coverage from the middle of one death hook.
  // Every actual captured Condition callback must have reached its own owner.
  if (writes.interaction_condition_consumer_active ||
      writes.interaction_condition_cursor != writes.condition_writes.size())
    fail("InteractionDeathCaptureNotDrained");
  const auto now = take(world.actual_phase_moment());
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase())
    phase = take(world.enter_phase_read_context(
        "interaction_death_capture_closure", "interaction", {now, now}, now));
  auto owner = take(world.complete_exact_time_bindings());
  if (!owner)
    fail("InteractionCompleteOwningLeaseRequired");
  take(take(read_native_interaction_world(world, owner->reads()))
           ->validate_session_life_in_capture(writes));
}
void finish_writes(WorldBundle &world,
                   const CompleteExactWorldReadBindings &access,
                   ExactPhaseWriteContext &writes, ExactArithmeticBudget b) {
  // The original consumer reads this still-live capture in actual mutation
  // order. It is idempotent for sources already adopted before Director review.
  take(consume_complete_growth_spatial_inputs_in_candidate(world, writes, b));
  take(consume_complete_growth_quiet_inputs_in_candidate(world, writes, b));
  adopt_growth_condition_inputs(world, writes, b);
  take(validate_pending_growth_releases(world, writes.pending_study_releases));
  if (writes.interaction_condition_cursor != writes.condition_writes.size())
    throw Failure{common_failure("InteractionDeathCaptureNotConsumed", {},
                                 ErrorKind::internal)
                      .error};
  validate_interaction_death_capture(world, writes);
  if (writes.pending_interaction_deaths &&
      writes.pending_interaction_deaths->has_pending())
    fail("InteractionDeathTailActualSealRequired");
  world.note_write_boundary();
  take(validate_actor_condition_capture(
      world.common, *world.actor_foundation, writes.condition_writes,
      writes.availability_writes, writes.condition_creations));
  const auto body = take(access.character_conditions.sources());
  const auto *spatial = body.fixed_spatial;
  if (&body.common != &world.common || !spatial ||
      &spatial->movement != &access.movement || !world.spatial_carriers ||
      &spatial->carriers != &*world.spatial_carriers ||
      !world.transport_registrations ||
      &spatial->registrations != &*world.transport_registrations)
    throw Failure{common_failure("CompleteSpatialCaptureOriginalOwnersRequired",
                                 {}, ErrorKind::internal)
                      .error};
  take(validate_growth_fixed_spatial_source_moment(*spatial, body.actual_now));
  // 群体登记/释放写Common
  // Carried根；选择只能来自准入目录，不能由optional存在反向启用。
  bool carried_capture = false;
  for (const auto &module : world.catalog->identity.module_versions) {
    if ((module.first == "group_transport" &&
         (module.second == "4" || module.second == "5" ||
          module.second == "6")) ||
        (module.first == "transport_manifest_bindings" && module.second == "2"))
      carried_capture = true;
  }
  if (carried_capture) {
    if (!world.spatial_carriers || !world.transport_registrations)
      fail("MissingSelectedSpatialCaptureRoots");
    take(validate_spatial_location_capture(
        world.common, access.movement, *world.spatial_carriers,
        *world.transport_registrations, writes.spatial_location_writes, b,
        spatial->births, spatial->military_actions, spatial->custody_releases,
        spatial->field_exits));
  } else
    take(validate_spatial_location_capture(
        world.common, access.movement, writes.spatial_location_writes, b,
        spatial->births, spatial->military_actions, spatial->custody_releases,
        spatial->field_exits));
  take(world.common.finish_actor_condition_capture());
  take(world.common.finish_actor_location_capture());
  world.note_write_boundary();
}
void stable_sources(const WorldBundle &world,
                    const CompleteExactWorldReadBindings &access,
                    const WorldMoment &current, ExactArithmeticBudget b) {
  const auto version = world.catalog->selected_household_version;
  if (bool(world.catalog->household_purchases) !=
          bool(world.catalog->household_retail_settlements) ||
      bool(version) != bool(world.catalog->household_purchases) ||
      bool(world.household_purchases) != bool(version) ||
      bool(world.household_retail_settlements) != bool(version))
    fail("HouseholdSelectedStatePairMismatch");
  if (version) {
    if (version != 2 && version != 3)
      fail("UnsupportedHouseholdPurchasesVersion");
    auto scope = take(world.enter_phase_read_context(
        "household_source_closure", "local_economy", {current, current},
        current));
    auto lease = take(world.complete_exact_time_bindings());
    const auto &fresh = lease->reads();
    if (!fresh.household_purchases || !fresh.household_retail)
      fail("HouseholdOriginalOwningContextRequired");
    const auto &purchase = *fresh.household_purchases;
    const auto &retail = *fresh.household_retail;
    if (&purchase.common != &world.common ||
        &purchase.state != &*world.household_purchases ||
        &purchase.catalog != world.catalog->household_purchases.get() ||
        &purchase.retail != &retail ||
        purchase.selected_household_version != version ||
        &retail.common != &world.common ||
        &retail.state != &*world.household_retail_settlements ||
        &retail.catalog != world.catalog->household_retail_settlements.get() ||
        !same(purchase.now, current, b) || !same(retail.now, current, b))
      fail("HouseholdCandidateOwningMismatch");
    take(validate_household_purchases(purchase));
    take(validate_household_retail(retail));
  } else if (access.household_purchases || access.household_retail)
    fail("UnexpectedHouseholdOwningContext");

  // 外层advance
  // lease可能生于片首；真实current相关adapter必须在本phase作用域重新拥有。
  {
    auto scope = take(world.enter_phase_read_context(
        "action_time_closure", "world_timeline", {current, current}, current));
    auto lease = take(world.complete_exact_time_bindings());
    const auto &fresh = lease->reads();
    if (&fresh.runtime != &access.runtime)
      fail("ExactRuntimeBindingMismatch");
    take(validate_action_time_closure(
        world.common, *world.action_elapsed, fresh.runtime.action_times,
        current, world.world_timeline->scale_ref, fresh.action_sources, b));
  }
  {
    auto scope = take(world.enter_phase_read_context(
        "facility_claim_closure", "facility_claims", {current, current},
        current));
    auto lease = take(world.complete_exact_time_bindings());
    const auto &fresh = lease->reads();
    if (&fresh.runtime != &access.runtime ||
        &fresh.facility_claims != &*world.facility_claims)
      fail("FacilityBindingCandidateMismatch");
    take(validate_facility_claims(fresh.facility_claims,
                                  fresh.runtime.facility_claims, current,
                                  fresh.facility_sources, b));
  }
  if (world.social.original_relation_version == 2) {
    auto scope = take(
        world.enter_phase_read_context("social_original_source_closure",
                                       "social", {current, current}, current));
    auto lease = take(world.complete_exact_time_bindings());
    if (!lease)
      fail("OriginalSocialCompleteOwningLeaseRequired");
    take(take(read_native_original_social_world(world, lease->reads()))
             ->validate_original_sources());
  } else if (access.original_social)
    fail("UnexpectedOriginalSocialOwningContext");
  if (take(selected_native_interaction_versions(world.catalog->identity))
          .exact_sources()) {
    auto scope = take(world.enter_phase_read_context(
        "interaction_original_source_closure", "interaction",
        {current, current}, current));
    auto lease = take(world.complete_exact_time_bindings());
    if (!lease)
      fail("InteractionCompleteOwningLeaseRequired");
    take(take(read_native_interaction_world(world, lease->reads()))
             ->validate_original_sources());
  }
}
} // namespace
Status consume_complete_growth_spatial_inputs_in_candidate(
    WorldBundle &world, ExactPhaseWriteContext &writes,
    ExactArithmeticBudget b,const GrowthPendingStartedProof* pending) {
  return guard<Done>([&] {
    required(world);
    if (!world.common.actor_condition_capture_identity() ||
        world.active_exact_write_context() != &writes)
      fail("GrowthSpatialInputSameCaptureRequired");
    const auto now = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase())
      phase = take(world.enter_phase_read_context(
          "growth_actual_spatial_adoption", "growth", {now, now}, now));
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner)
      fail("CompleteGrowthOwningLeaseRequired");
    auto body = take(owner->reads().character_conditions.sources());
    if (&body.common != &world.common || body.growth != &world.growth ||
        body.elapsed != &*world.action_elapsed ||
        !same(body.actual_now, now, b))
      fail("CompleteGrowthCandidateBindingMismatch");
    NativeDomainReadVersionSource versions(
        world, owner->reads().military_formations,
        owner->reads().military_returns,
        owner->reads().military_supply_periods);
    auto reads = world.make_current_read_stamp_reader(
        versions, {4096, 4096, 1024 * 1024});
    auto original = world.services.growth_access();
    auto market = take(read_native_growth_market_exact_sources(world));
    GrowthFixedCaptureServices services{
        *reads,
        *world.read_stamps,
        versions,
        original,
        owner->growth_start_participation_writer(),
        market ? &*market : nullptr};
    auto effects = take(consume_growth_spatial_inputs_in_candidate(
        world.common, world.growth, body, *world.action_elapsed, services,
        [&world](const StableId &action) {
          return release_action(world, action);
        },pending));
    merge_growth_input_effects(world, writes, std::move(effects));
    return Done{};
  });
}
Status consume_complete_growth_quiet_inputs_in_candidate(
    WorldBundle &world, ExactPhaseWriteContext &writes,
    ExactArithmeticBudget b,const GrowthPendingStartedProof* pending) {
  return guard<Done>([&] {
    required(world);
    if (!world.catalog->actor_social_occupancy_selected)
      return Done{};
    if (!world.common.actor_condition_capture_identity() ||
        world.active_exact_write_context() != &writes)
      fail("GrowthQuietInputSameCaptureRequired");
    const auto now = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase())
      phase = take(world.enter_phase_read_context(
          "growth_actual_quiet_adoption", "growth", {now, now}, now));
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner)
      fail("CompleteGrowthOwningLeaseRequired");
    auto body = take(owner->reads().character_conditions.sources());
    if (&body.common != &world.common || body.growth != &world.growth ||
        body.elapsed != &*world.action_elapsed ||
        !same(body.actual_now, now, b))
      fail("CompleteGrowthCandidateBindingMismatch");
    NativeDomainReadVersionSource versions(
        world, owner->reads().military_formations,
        owner->reads().military_returns,
        owner->reads().military_supply_periods);
    auto reads = world.make_current_read_stamp_reader(
        versions, {4096, 4096, 1024 * 1024});
    auto original = world.services.growth_access();
    auto market = take(read_native_growth_market_exact_sources(world));
    GrowthFixedCaptureServices services{
        *reads,
        *world.read_stamps,
        versions,
        original,
        owner->growth_start_participation_writer(),
        market ? &*market : nullptr};
    auto effects = take(consume_growth_quiet_inputs_in_candidate(
        world.common, world.growth, body, *world.action_elapsed, services,
        [&world](const StableId &action) {
          return release_action(world, action);
        },pending));
    merge_growth_input_effects(world, writes, std::move(effects));
    return Done{};
  });
}
Status consume_complete_growth_condition_inputs_in_candidate(
    WorldBundle &world, ExactPhaseWriteContext &writes,
    ExactArithmeticBudget budget) {
  return guard<Done>([&] {
    required(world);
    if (world.active_exact_write_context() != &writes ||
        !world.common.actor_condition_capture_identity())
      throw Failure{common_failure("CompleteActualConditionCaptureRequired", {},
                                   ErrorKind::internal)
                        .error};
    adopt_growth_condition_inputs(world, writes, budget);
    return Done{};
  });
}
#include "pending_growth_started.inc"

// 同候选唯一捕获拥有待绑定证明；关闭前必须完成所有原结果联接。
struct CompleteExactCommandCapture::State : CheckpointAllocatedObject {
  std::unique_ptr<GrowthPendingStartedProof> pending_growth;
  WorldBundle &world;
  AdvanceDetails effects;
  // Declared before its borrowed write context, so it outlives that context.
  std::unique_ptr<InteractionSelectedResponsePending> selected_response;
  std::unique_ptr<InteractionSessionDeathPending> pending_deaths;
  std::unique_ptr<InteractionActionConfirmationPending> pending_confirmation;
  ExactPhaseWriteContext writes;
  std::unique_ptr<CompleteExactWorldBindingOwner> owners;
  // The pending token requires this exact context address through the true
  // Common seal. It owns no persisted state and never escapes the candidate.
  std::unique_ptr<CompleteExactWorldBindingOwner> private_shop_owner;
  const NativeHouseholdFoundationLease *private_shop_foundation = nullptr;
  const std::vector<FacilityHolderRegistration> *private_shop_holders = nullptr;
  std::optional<ResourceExecutionCoverageBoundary> private_shop_coverage;
  std::optional<PendingPrivateShopCancellation> private_shop_cancellation;
  CheckpointCopyLease private_shop_operation_memory;
  StableId private_shop_cancelled_operation;
  std::unique_ptr<WorldExactWriteScope> scope;
  bool closed = false, confirmation_bound = false;
  State(WorldBundle &w, std::unique_ptr<CompleteExactWorldBindingOwner> owned)
      : world(w), writes(effects), owners(std::move(owned)) {
    take(validate_native_interaction_response_layout(*world.catalog,
                                                     world.interaction));
    if (take(selected_native_interaction_response_version(
            world.catalog->identity))) {
      if (!world.catalog->interaction)
        fail("InteractionResponseCatalogRequired");
      selected_response = take(InteractionSelectedResponsePending::create(
          world.common, world.interaction, *world.catalog->interaction));
      writes.selected_interaction_response = selected_response.get();
      if (interaction_action_confirmation_schema_version(
              world.catalog->interaction->response_sources_version)) {
        pending_confirmation =
            take(InteractionActionConfirmationPending::create(
                world.common, world.interaction, *world.catalog->interaction));
        writes.pending_interaction_confirmation = pending_confirmation.get();
      }
    }
    begin_writes(world);
    take(begin_complete_command_publication(world.common));
    scope.reset(new WorldExactWriteScope(world, writes));
    if (selected_response) {
      pending_deaths = take(InteractionSessionDeathPending::create(
          world.common, world.interaction, *world.catalog->interaction));
      writes.pending_interaction_deaths = pending_deaths.get();
    }
  }
};
CompleteExactCommandCapture::CompleteExactCommandCapture(
    std::unique_ptr<State> state)
    : state_(std::move(state)) {}
CompleteExactCommandCapture::~CompleteExactCommandCapture() = default;
Outcome<std::unique_ptr<CompleteExactCommandCapture>>
CompleteExactCommandCapture::begin(WorldBundle &world) {
  return guard<std::unique_ptr<CompleteExactCommandCapture>>([&] {
    required(world);
    if (world.active_exact_write_context() ||
        world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureAlreadyActive");
    if (!world.read_stamps)
      fail("CompleteCommandReadStampRequired");
    std::unique_ptr<CompleteExactWorldBindingOwner> owners;
    {
      // 构图只借当前短票；先退票再开启原写捕获，业务回调仍各自取得新票。
      const auto now = take(world.actual_phase_moment());
      auto phase = take(world.enter_phase_read_context(
          "complete_command_binding", "world_timeline", {now, now}, now));
      owners = take(world.complete_exact_time_bindings());
      if (!owners)
        fail("CompleteCommandOwningLeaseRequired");
    }
    std::unique_ptr<State> state(new State(world, std::move(owners)));
    return std::unique_ptr<CompleteExactCommandCapture>(
        new CompleteExactCommandCapture(std::move(state)));
  });
}
bool CompleteExactCommandCapture::bound_to(
    const WorldBundle &world) const noexcept {
  return state_ && !state_->closed && &state_->world == &world &&
         world.active_exact_write_context() == &state_->writes;
}
// 通用原开工入口只按编译owner选择；不是交易或任何动作种类特判。
Status CompleteExactCommandCapture::prepare_pending_growth_start(const ActionCoreView& action) {
  return guard<Done>([&]{
    if(!state_||!bound_to(state_->world))fail("CompleteCommandCaptureNotLive");
    auto& world=state_->world;
    if(!world.catalog->complete_exact_time_runtime)fail("CompleteExactRuntimeRequired");
    const ActionTimeRegistration* selected=nullptr;
    for(const auto& row:world.catalog->complete_exact_time_runtime->action_times)
      if(row.action_kind==action.kind) {
        if(selected)fail("DuplicateActualActionTimeRegistration");selected=&row;
      }
    if(!selected)fail("CompleteActualActionTimeRegistrationRequired");
    if(selected->owner_domain!="growth")return Done{};
    if(state_->pending_growth)state_->pending_growth->state_->reject("pending_start_duplicate");
    auto input=std::unique_ptr<GrowthPendingStartedProof::State>(
      new GrowthPendingStartedProof::State(world,action));
    state_->pending_growth=std::unique_ptr<GrowthPendingStartedProof>(
      new GrowthPendingStartedProof(std::move(input)));
    return Done{};
  });
}
// 原采用本身可改人物占用；只在该采用完整成功后开始粘性资源监视。
Status CompleteExactCommandCapture::observe_pending_growth_adoption(const StableId& id) {
  return guard<Done>([&]{
    if(!state_||!bound_to(state_->world))fail("CompleteCommandCaptureNotLive");
    if(!state_->pending_growth)return Done{};
    auto& pending=*state_->pending_growth->state_;
    if(id!=pending.action.id||pending.stage!=GrowthPendingStartedProof::State::Stage::prospective)
      pending.reject("pending_start_adoption_mismatch");
    auto boundary=state_->world.common.validate_actor_condition_savepoint_boundary(pending.adoption_boundary);
    if(!boundary.ok)pending.reject("pending_start_adoption_lifetime_changed");
    pending.stage=GrowthPendingStartedProof::State::Stage::adopted;
    pending.validate();
    std::vector<ResourceKey> keys{{ResourceKey::Kind::action,id}};
    for(const auto& actor:pending.action.participants)keys.push_back({ResourceKey::Kind::actor,actor});
    pending.resources=take(CommonCaptureReadSetProof::capture(state_->world.common,keys));
    return Done{};
  });
}
// 原业务头确已追加后才加第二见证，不覆盖已监视的人物/行动生命周期。
Status CompleteExactCommandCapture::observe_pending_growth_business(const StableId& id) {
  return guard<Done>([&]{
    if(!state_||!bound_to(state_->world))fail("CompleteCommandCaptureNotLive");
    if(!state_->pending_growth)return Done{};
    auto& pending=*state_->pending_growth->state_;
    if(id!=pending.action.id||pending.stage!=GrowthPendingStartedProof::State::Stage::adopted)
      pending.reject("pending_start_business_mismatch");
    pending.validate();
    const auto* header=state_->world.common.state().facts.find(id);
    if(!header)pending.reject("pending_start_business_source_changed");
    pending.source=take(CommonCaptureReadSetProof::capture(state_->world.common,{},
      {{header->domain,header->kind,header->id}}));
    pending.stage=GrowthPendingStartedProof::State::Stage::business;
    return Done{};
  });
}
// 真实外层ActionStarted与不可变旧世界联接后才允许消费者借读证明。
Status CompleteExactCommandCapture::identify_pending_growth_start(const WorldBundle* before,
    const StableId& actor,Integer sequence,const ReceiptDetails& details) {
  return guard<Done>([&]{
    if(!state_||!bound_to(state_->world))fail("CompleteCommandCaptureNotLive");
    if(!state_->pending_growth)return Done{};
    auto& pending=*state_->pending_growth->state_;
    const auto* started=std::get_if<ActionStarted>(&details.data);
    if(pending.stage!=GrowthPendingStartedProof::State::Stage::business || !started ||
       started->action_id!=pending.action.id || actor!=pending.action.actor_id || sequence<1 ||
       !before || before==&state_->world || before->catalog.get()!=state_->world.catalog.get())
      pending.reject("pending_start_original_binding_mismatch");
    pending.validate();
    if(before->common.action(pending.action.id)||before->common.state().facts.find(pending.action.id))
      pending.reject("pending_start_not_new");
    take(read_growth_started_business_source(before->growth,state_->world.growth,
      state_->world.common,*state_->world.common.action(pending.action.id),pending.at));
    pending.before=before;pending.actor=actor;pending.sequence=sequence;
    pending.stage=GrowthPendingStartedProof::State::Stage::identified;
    return Done{};
  });
}
// 原封存后调用唯一旧binder，并核其真实Started和原输入切面；失败仍丢弃整个候选。
Status CompleteExactCommandCapture::bind_growth_command_result(const WorldBundle* before,
    const StableId& actor,Integer sequence,const CommitResult& result) {
  return guard<Done>([&]{
    if(!state_||!bound_to(state_->world))fail("CompleteCommandCaptureNotLive");
    auto* pending=state_->pending_growth?state_->pending_growth->state_.get():nullptr;
    if(pending) {
      if(pending->stage!=GrowthPendingStartedProof::State::Stage::identified ||
         pending->before!=before || pending->actor!=actor || pending->sequence!=sequence)
        pending->reject("pending_start_original_binding_mismatch");
      pending->validate();
    }
    take(bind_complete_growth_command_result(before,state_->world,actor,sequence,result));
    if(!pending)return Done{};
    pending->validate(true);
    const LifeFixedServiceBoundary* started=nullptr;
    for(const auto& row:state_->world.growth.fixed_service_boundaries.rows())
      if(row.second.action_ref==pending->action.id) {
        if(started||row.second.kind!=LifeFixedBoundaryKind::started)
          pending->reject("pending_start_original_binding_mismatch");
        started=&row.second;
      }
    const auto* command=started?std::get_if<LifeActualCommand>(&started->cause):nullptr;
    if(!started||!command||command->actor_id!=actor||
       command->command_binding.request_seq!=sequence||
       command->command_binding.result_id!=result.result_id||
       command->command_binding.command_kind!=result.kind||!same(started->occurred_at,pending->at,{}))
      pending->reject("pending_start_original_binding_mismatch");
    if(started->input_snapshot.actors.size()!=pending->actors.size())
      pending->reject("pending_start_participant_mismatch");
    for(const auto& old:pending->actors) {
      const LifeFixedActorInputs* input=nullptr;
      for(const auto& row:started->input_snapshot.actors)if(row.actor_id==old.condition.actor_id) {
        if(input)pending->reject("pending_start_participant_mismatch");input=&row;
      }
      if(!input||input->capability_frames.empty())pending->reject("pending_start_participant_mismatch");
      for(const auto& frame:input->capability_frames)
        if(!pending_head_equal(frame.condition_head_snapshot,old.condition))
          pending->reject("pending_start_condition_changed");
      if(!old.location || !(PendingLocationCut(input->location_snapshot)==*old.location) ||
         input->actual_location_source_refs.size()!=old.spatial.size())
        pending->reject("pending_start_spatial_changed");
      for(std::size_t i=0;i<old.spatial.size();++i) {
        const auto& stored=input->actual_location_source_refs[i];
        if(const auto* actual=std::get_if<SourceRef>(&stored)) {
          if(!pending_source_equal(*actual,old.spatial[i].source))pending->reject("pending_start_spatial_changed");
        } else pending->reject("pending_start_spatial_changed");
      }
    }
    const auto& quiet=started->input_snapshot.specific.kind==0?
      started->input_snapshot.specific.growth_fact.original_quiet_or_rest_basis:
      started->input_snapshot.specific.life_activity.original_quiet_or_rest_basis;
    if(quiet) {
      const PendingQuietCut* original=nullptr;
      for(const auto& old:pending->actors)if(old.condition.actor_id==actor && old.quiet)original=&*old.quiet;
      if(!original||original->quiet!=quiet->is_quiet||
         original->original_sources.size()!=quiet->actual_conflict_source_refs.size())
        pending->reject("pending_start_quiet_changed");
      for(std::size_t i=0;i<original->original_sources.size();++i) {
        const auto* actual=std::get_if<SourceRef>(&quiet->actual_conflict_source_refs[i]);
        if(!actual||!pending_source_equal(*actual,original->original_sources[i]))pending->reject("pending_start_quiet_changed");
      }
    }
    pending->stage=GrowthPendingStartedProof::State::Stage::consumed;
    return Done{};
  });
}
Outcome<ReceiptDetails> CompleteExactCommandCapture::apply_private_shop_command(
    const StableId &actor, const PreparedDomainOp &op) {
  return guard<ReceiptDetails>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    auto &world = state_->world;
    const auto *command = std::get_if<PrivateShopCommand>(&op.command.data);
    const auto *quote = std::get_if<PrivateShopCommandQuote>(&op.quote.data);
    if (!command || !quote ||
        (!std::holds_alternative<PrivateShopOperate>(*command) &&
         !std::holds_alternative<PrivateShopOperateWithCustody>(*command) &&
         !std::holds_alternative<PrivateShopCancelOperation>(*command)) ||
        op.command.kind != private_shop_command_kind(*command))
      fail("InvalidPrivateShopCommand");
    if (state_->private_shop_owner || state_->private_shop_cancellation ||
        state_->private_shop_coverage)
      fail("CompletePrivateShopCommandAlreadyApplied");
    take(command_common_guard(world, actor,
                              private_shop_command_kind(*command)));
    const auto &identity = world.common.state().identity;
    if (quote->actor_id != actor ||
        quote->source_identity.world_id != identity.world_id ||
        quote->source_identity.execution_epoch != identity.execution_epoch ||
        quote->source_identity.revision != identity.revision ||
        quote->source_identity.tick != identity.tick ||
        !compare_boundary(encode_private_shop_command(*command),
                          encode_private_shop_command(quote->command))
             .exact())
      fail("StalePrivateShopQuote");
    const auto current =
        take(prepare_private_shop_world(world, actor, *command));
    if (!compare_boundary(encode_private_shop_quote(current),
                          encode_private_shop_quote(*quote))
             .exact())
      fail("StalePrivateShopQuote");
    if (!world.read_stamps)
      fail("CompleteCommandReadStampRequired");
    const auto now = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase())
      phase = take(world.enter_phase_read_context("private_shop_command_apply",
                                                  "commerce", {now, now}, now));
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner)
      fail("CompletePrivateShopOwningLeaseRequired");
    const auto &reads = owner->reads();
    const auto *foundation = reads.household_foundation;
    if (!foundation || !foundation->operations() || !foundation->shops() ||
        reads.private_shop_operations != foundation->operations())
      fail("CompletePrivateShopFoundationRequired");
    take(foundation->validate_bindings());
    if (!world.common.resource_execution_capture_identity())
      state_->private_shop_coverage.emplace(
          take(begin_resource_execution_coverage(world.common)));
    NativePrivateShopMarketCommandContext context{
        world, *foundation, reads.facility_holder_handlers, state_->writes};
    if (std::holds_alternative<PrivateShopOperate>(*command) ||
        std::holds_alternative<PrivateShopOperateWithCustody>(*command)) {
      const auto *admission =
          std::get_if<PrivateShopOperationQuote>(&current.data);
      if (!admission)
        fail("InvalidPrivateShopOperationQuote");
      // 实际营业和明确托管接受共享同一私有候选；任何后腿失败都由外层撤销全候选。
      auto action =
          take(start_native_private_shop_market_operation_in_candidate(
              context, *admission));
      if (const auto *explicit_choice =
              std::get_if<PrivateShopOperateWithCustody>(command)) {
        const auto *custody = foundation->private_custody();
        if (!custody || !world.private_shop_household_custody ||
            &custody->state != &*world.private_shop_household_custody)
          fail("PrivateShopExplicitCustodyContextRequired");
        auto policy = take(read_private_shop_explicit_custody_policy(
            *custody, *explicit_choice));
        take(accept_private_shop_household_custody_in_candidate(
            world.common, *world.private_shop_household_custody, *custody,
            action, policy, actor));
        take(validate_private_shop_household_custody_sources(*custody));
      }
      take(validate_private_shops(*foundation->shops()));
      if (world.growth_market_capture.open_scopes ||
          world.growth_market_capture.unbound_business)
        fail("CompletePrivateShopMarketCaptureUnclosed");
      if (state_->private_shop_coverage) {
        take(finish_resource_execution_coverage(
            world.common, *state_->private_shop_coverage));
        state_->private_shop_coverage.reset();
      }
      return ReceiptDetails{ActionStarted{std::move(action)}, {}};
    }
    const auto &cancel = std::get<PrivateShopCancelOperation>(*command);
    auto pending =
        take(cancel_native_private_shop_market_operation_in_candidate(
            context, actor, cancel.action_id));
    auto result = take(read_pending_private_shop_cancellation_result(pending));
    // The same owner/context and both original captures remain alive until
    // commit_tail provides the actual source; no fresh context can replace it.
    state_->private_shop_foundation = foundation;
    state_->private_shop_holders = &reads.facility_holder_handlers;
    state_->private_shop_owner = std::move(owner);


    state_->private_shop_operation_memory =
        CheckpointCopyLease::reserve();
    state_->private_shop_cancelled_operation =
        checkpoint_fresh_string_copy(cancel.action_id);
    state_->private_shop_cancellation.emplace(std::move(pending));
    InteractionEffect effect;
    effect.settlement_result = std::move(result);
    return ReceiptDetails{std::move(effect), {}};
  });
}
Status CompleteExactCommandCapture::bind_private_shop_command_result(
    const SourceRef &result) {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    if (!state_->private_shop_cancellation)
      return Done{};
    auto &world = state_->world;
    if (!state_->private_shop_owner || !state_->private_shop_foundation ||
        !state_->private_shop_holders || !world.read_stamps)
      fail("CompletePrivateShopCancellationOwnerRequired");
    const auto now = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase())
      phase = take(world.enter_phase_read_context(
          "private_shop_actual_command_seal", "commerce", {now, now}, now));
    NativePrivateShopMarketCommandContext context{
        world, *state_->private_shop_foundation, *state_->private_shop_holders,
        state_->writes};
    take(bind_native_private_shop_market_cancellation_in_candidate(
        context, std::move(*state_->private_shop_cancellation), {result, 1}));
    state_->private_shop_cancellation.reset();
    take(resume_complete_private_shop_dialogue_in_candidate(
        world, state_->private_shop_cancelled_operation));
    if (world.growth_market_capture.open_scopes ||
        world.growth_market_capture.unbound_business)
      fail("CompletePrivateShopMarketCaptureUnclosed");
    if (state_->private_shop_coverage) {
      take(finish_resource_execution_coverage(world.common,
                                              *state_->private_shop_coverage));
      state_->private_shop_coverage.reset();
    }
    state_->private_shop_foundation = nullptr;
    state_->private_shop_holders = nullptr;
    state_->private_shop_owner.reset();
    {
      StableId released_operation;
      state_->private_shop_cancelled_operation.swap(released_operation);
    }
    state_->private_shop_operation_memory = {};
    return Done{};
  });
}
// 封存前只给已识别的新开工局部证明；原旧输入与死亡释放仍按原顺序执行。
Status CompleteExactCommandCapture::settle() {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    auto &world = state_->world;
    auto &writes = state_->writes;
    const auto* pending=state_->pending_growth.get();
    if(pending && pending->state_->stage!=GrowthPendingStartedProof::State::Stage::consumed)
      pending->state_->validate();
    take(consume_complete_growth_spatial_inputs_in_candidate(world, writes, {}, pending));
    take(consume_complete_growth_quiet_inputs_in_candidate(world, writes, {}, pending));
    adopt_growth_condition_inputs(world, writes, {}, pending);
    take(
        validate_pending_growth_releases(world, writes.pending_study_releases));
    validate_interaction_death_capture(world, writes);
    return Done{};
  });
}
Status CompleteExactCommandCapture::finalize_selected_interaction_response(
    const SourceRef &result) {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    auto &world = state_->world;
    if (!state_->selected_response)
      fail("InteractionOriginalResponseCaptureRequired");
    if (state_->writes.selected_interaction_response !=
        state_->selected_response.get())
      fail("InteractionResponseCaptureMismatch");
    const auto actual = take(world.actual_phase_moment());
    auto access = world.services.interaction_access();
    take(finalize_interaction_selected_response_in_candidate(
        world.common, world.interaction, *world.catalog->interaction, access,
        *state_->selected_response, result, actual));
    // Borrow the now committed original row. This fails if a real choice was
    // left unfinalized; a command with no selected response returns null.
    take(state_->selected_response->read_committed_response(
        world.common, world.interaction, *world.catalog->interaction));
    world.note_write_boundary();
    return Done{};
  });
}
Status CompleteExactCommandCapture::bind_interaction_session_death_result(
    const SourceRef &result) {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    if (state_->writes.pending_interaction_deaths !=
        state_->pending_deaths.get())
      fail("InteractionDeathCaptureOwnerMismatch");
    if (!state_->pending_deaths || !state_->pending_deaths->has_pending())
      return Done{};
    auto &world = state_->world;
    const auto now = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase())
      phase = take(world.enter_phase_read_context(
          "interaction_death_actual_seal", "interaction", {now, now}, now));
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner)
      fail("InteractionCompleteOwningLeaseRequired");
    take(take(read_native_interaction_world(world, owner->reads()))
             ->bind_actual_death_result_in_candidate(world, state_->writes,
                                                     result));
    return Done{};
  });
}
// 原关闭口拒绝任何未完成真实Started绑定的证明，不留下可复用待办。
Status CompleteExactCommandCapture::finish() {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    if(state_->pending_growth && state_->pending_growth->state_->stage!=GrowthPendingStartedProof::State::Stage::consumed)
      state_->pending_growth->state_->reject("pending_start_unresolved");
    if (state_->selected_response &&
        state_->selected_response->has_response() &&
        !state_->selected_response->consumed())
      fail("InteractionResponseFinalizerRequired");
    if (state_->pending_confirmation &&
        state_->pending_confirmation->has_confirmation() &&
        !state_->confirmation_bound)
      fail("InteractionConfirmationActualSealRequired");
    if (state_->private_shop_cancellation || state_->private_shop_owner ||
        state_->private_shop_coverage)
      fail("CompletePrivateShopActualCommandSealRequired");
    if (!state_->world.read_stamps)
      fail("CompleteCommandReadStampRequired");
    std::unique_ptr<CompleteExactWorldBindingOwner> final_owner;
    {
      // 命令可能已推进真实时刻；收尾重新借同刻原图，不能复用开始时的快照。
      const auto now = take(state_->world.actual_phase_moment());
      auto phase = take(state_->world.enter_phase_read_context(
          "complete_command_finish_binding", "world_timeline", {now, now},
          now));
      final_owner = take(state_->world.complete_exact_time_bindings());
      if (!final_owner)
        fail("CompleteCommandOwningLeaseRequired");
    }
    // 实际 Common seal 与全部原来源 binder 已完成；零时命令新生的同刻
    // 来源也必须沿原完整图结清。不开日历、不推进时钟、不直接补采集游标；
    // 原图逐叶重读、成果反验、回边拒绝和有限预算仍由唯一调度者执行。
    const auto now = take(state_->world.actual_phase_moment());
    const bool boundary =
        take(compare_fractions(now.sub_half_day, CanonicalFraction{})) == 0;
    take(consume_exact_due_events(final_owner->reads().runtime.phase_graph,
                                  state_->world, now, boundary, state_->writes,
                                  {}, {}, nullptr));
    finish_writes(state_->world, final_owner->reads(), state_->writes, {});
    state_->scope.reset();
    state_->closed = true;
    return Done{};
  });
}
Status CompleteExactCommandCapture::bind_interaction_action_confirmation_result(
    const SourceRef &result) {
  return guard<Done>([&] {
    if (!state_ || !bound_to(state_->world) ||
        !state_->world.common.actor_condition_capture_identity())
      fail("CompleteCommandCaptureNotLive");
    if (state_->writes.pending_interaction_confirmation !=
        state_->pending_confirmation.get())
      fail("InteractionConfirmationCaptureOwnerMismatch");
    if (!state_->pending_confirmation)
      return Done{};
    auto &world = state_->world;
    take(bind_interaction_action_confirmation_result_in_candidate(
        world.common, world.interaction, *world.catalog->interaction,
        *state_->pending_confirmation, result));
    state_->confirmation_bound = true;
    if (state_->pending_confirmation->has_confirmation())
      world.note_write_boundary();
    return Done{};
  });
}
Outcome<std::optional<ExactPeriodContext>>
read_complete_exact_open_period(const WorldBundle &world,
                                ExactArithmeticBudget b) {
  return guard<std::optional<ExactPeriodContext>>([&] {
    if (!world.catalog || !world.catalog->exact_time ||
        !exact_complete_selected(*world.catalog->exact_time) ||
        !world.world_timeline)
      fail("CompleteExactTimelineRequired");
    take(
        validate_timeline_value(world.common.tick(), *world.world_timeline, b));
    if (world.world_timeline->boundary_state == PeriodBoundaryState::ready)
      return std::optional<ExactPeriodContext>{};
    const auto current =
        take(read_world_moment(world.common.tick(), *world.world_timeline, b));
    return std::optional<ExactPeriodContext>{period_at(world, current, false)};
  });
}
Status
validate_complete_exact_time_world(const WorldBundle &world,
                                   const CompleteExactWorldReadBindings &access,
                                   ExactArithmeticBudget b) {
  return guard<Done>([&] {
    required(world);
    auto growth_selection =
        take(read_complete_growth_phase_version(*world.catalog->exact_time));
    if (!world.catalog->growth ||
        !world.catalog->growth->complete_study_sources ||
        !world.catalog->growth->complete_exact_period_sources ||
        world.catalog->growth->complete_exact_command_cuts !=
            (growth_selection >= 2) ||
        world.catalog->growth->complete_exact_rest_mouth_sources !=
            (growth_selection >= 3) ||
        world.catalog->growth->complete_capability_application_chain !=
            (growth_selection == 4))
      fail("GrowthExactCatalogSelectionMismatch");
    std::size_t growth_markers = 0;
    for (const auto &row : world.catalog->identity.module_versions)
      if (row.first == "growth_fixed_service_time2") {
        ++growth_markers;
        if (row.second != std::to_string(growth_selection))
          fail("GrowthExactCatalogSelectionMismatch");
      }
    if (growth_markers != (growth_selection >= 2 ? 1u : 0u))
      fail("GrowthExactCatalogSelectionMismatch");
    if (access.military_formations && !access.military_formations->bound_to(
                                          world.common.state(), world.military))
      throw Failure{common_failure("CompleteFormationCandidateMismatch", {},
                                   ErrorKind::internal)
                        .error};
    if (access.military_returns &&
        !access.military_returns->bound_to(world.common, world.military))
      throw Failure{common_failure("CompleteReturnCandidateMismatch", {},
                                   ErrorKind::internal)
                        .error};
    if (access.military_supply_periods &&
        (!world.production_cycles ||
         !access.military_supply_periods->bound_to(world.common,
                                                   *world.production_cycles)))
      throw Failure{common_failure("CompleteMilitaryPeriodCandidateMismatch",
                                   {}, ErrorKind::internal)
                        .error};
    if (take(access.character_conditions.sources()).military_supply_periods !=
        access.military_supply_periods)
      throw Failure{common_failure("CompleteMilitaryPeriodOwningMismatch", {},
                                   ErrorKind::internal)
                        .error};
    if (&access.facility_claims != &*world.facility_claims)
      fail("FacilityBindingCandidateMismatch");
    const auto &expected = world.catalog->exact_time->scale_ref;
    const auto &actual = world.world_timeline->scale_ref;
    if (expected.definition_id != actual.definition_id ||
        expected.version != actual.version ||
        expected.units_per_half_day != actual.units_per_half_day)
      fail("ExactTimelineScaleIdentityMismatch");
    take(
        validate_timeline_value(world.common.tick(), *world.world_timeline, b));
    take(validate_exact_phase_selection(access.runtime.phase_graph, world));
    const auto current =
        take(read_world_moment(world.common.tick(), *world.world_timeline, b));
    const auto initial = initial_at(world);
    if (take(compare_world_moments(current, initial, b)) < 0)
      fail("TimelineBeforeInitialOrigin");
    const auto &timeline = *world.world_timeline;
    const bool initial_ready =
        timeline.boundary_state == PeriodBoundaryState::ready &&
        same(current, initial, b);
    if (initial_ready) {
      if (timeline.opening_receipt_ref || timeline.close_receipt_ref)
        fail("InitialReadyHasPeriodReceipt");
    } else {
      if (!timeline.opening_receipt_ref ||
          *timeline.opening_receipt_ref != timeline.period_id)
        fail("MissingExactOpeningIndex");
      if (timeline.boundary_state == PeriodBoundaryState::ready) {
        if (!timeline.close_receipt_ref ||
            *timeline.close_receipt_ref != timeline.period_id)
          fail("MissingExactClosingIndex");
      } else if (timeline.close_receipt_ref)
        fail("OpenExactPeriodHasClosure");
      take(validate_exact_registered_period_sources(
          access.runtime.phase_graph, world,
          period_at(world, current,
                    timeline.boundary_state == PeriodBoundaryState::ready),
          timeline.boundary_state));
    }
    take(validate_exact_phase_sources(access.runtime.phase_graph, world,
                                      current));
    if (!access.director.read || !access.director.write)
      fail("CompleteDirectorOriginalFactoryRequired");
    {
      auto scope = take(world.enter_phase_read_context(
          "director_source_closure", "director", {current, current}, current));
      take(validate_complete_director_sources(world, access.director));
    }
    // 命令保留闭包的真互动 owner 也须当前票；不借前一 Director 票或跨阶段复用。
    {
      auto scope = take(world.enter_phase_read_context(
          "command_source_closure", "common", {current, current}, current));
      take(validate_complete_command_source_pins(world));
    }
    stable_sources(world, access, current, b);
    take(validate_no_exact_due(access.runtime.phase_graph, world, current));
    return Done{};
  });
}
Outcome<WorldMoment> prepare_complete_exact_advance_target(
    const WorldBundle &world, const CompleteExactWorldReadBindings &access,
    const std::optional<WorldMoment> &requested, ExactArithmeticBudget b) {
  return guard<WorldMoment>([&] {
    required(world);
    const auto current =
        take(read_world_moment(world.common.tick(), *world.world_timeline, b));
    // 不先构造current+1：分数靠近Integer尾端时该中间值可溢出，而下一原宏观界仍合法。
    if (current.half_day_tick == std::numeric_limits<Integer>::max())
      fail("WorldMomentOverflow");
    const auto permit =
        requested ? *requested : WorldMoment{current.half_day_tick + 1, {}};
    take(encode_world_moment(permit, b));
    if (take(compare_world_moments(current, permit, b)) >= 0)
      fail("InvalidExactAdvanceTarget");
    auto duration = take(elapsed_world_duration(current, permit, b));
    if (take(compare_fractions(duration.value,
                               take(normalize_fraction("1", "1", b)), b)) > 0)
      fail("InvalidExactAdvanceTarget");
    return take(select_exact_slice_end(access.runtime.phase_graph, world,
                                       current, permit, b));
  });
}
Status open_complete_exact_period_in_candidate(
    WorldBundle &world, const CompleteExactWorldReadBindings &access,
    ExactArithmeticBudget b) {
  return guard<Done>([&] {
    take(validate_complete_exact_time_world(world, access, b));
    if (world.world_timeline->boundary_state != PeriodBoundaryState::ready)
      fail("ExactPeriodAlreadyOpen");
    if (world.field_battle) {
      const auto gate = take(read_field_world_advance_gate(
          *world.field_battle, world.common.state().metadata.player_actor_id,
          world.common.tick()));
      if (gate.reason != FieldWorldAdvanceReason::allowed)
        throw Failure{
            common_failure(gate.reason == FieldWorldAdvanceReason::player_input
                               ? "FieldBattlePlayerInputRequired"
                               : "FieldBattleLocalSettlementRequired",
                           gate.battle_id)
                .error};
    }
    const auto at =
        take(read_world_moment(world.common.tick(), *world.world_timeline, b));
    if (at.half_day_tick == std::numeric_limits<Integer>::max())
      fail("WorldTickOverflow");
    AdvanceDetails opening_effects;
    ExactPhaseWriteContext writes{opening_effects};
    begin_writes(world);
    WorldExactWriteScope capture(world, writes);
    auto id = take(world.common.next_id("world_period"));
    ExactPeriodContext period{std::move(id), at, at.half_day_tick + 1};
    auto sources = take(open_exact_registered_period(access.runtime.phase_graph,
                                                     world, period, writes));
    if (sources.empty())
      fail("MissingCompletePeriodOwner");
    // 该ID仅索引上面实际owner的持久opening，不新增供给/成果表，也不占玩家request
    // seq。
    world.world_timeline->period_id = period.period_id;
    world.world_timeline->opening_receipt_ref = period.period_id;
    world.world_timeline->close_receipt_ref.reset();
    world.world_timeline->boundary_state = PeriodBoundaryState::open;
    // 所有真实经济/军务 Opening 已落头后，胜军才能续同旅程的原余程。
    // 不把终局 local 秒当下一 M，不预付第一正片费用；失败仍撤全部 Opening。
    if (world.field_battle) {
      take(resume_native_never_admitted_field_boundaries_in_candidate(world, writes, true));
      take(resume_native_field_winners_at_opening_in_candidate(world, writes));
    }
    // Opening产生的真实同刻源必须在本候选消费；不能把它推到首个正片而重复领取全期。
    take(consume_exact_due_events(access.runtime.phase_graph, world, at, true,
                                  writes, {}, b));
    finish_writes(world, access, writes, b);
    take(collect_native_complete_director_wakes_in_candidate(world));
    take(validate_complete_exact_time_world(world, access, b));
    return Done{};
  });
}
Outcome<AdvanceDetails> advance_complete_exact_world(
    WorldBundle &world, const CompleteExactWorldReadBindings &access,
    const std::optional<WorldMoment> &requested,
    ExactPhaseTechnicalBudget technical, ExactArithmeticBudget b) {
  return guard<AdvanceDetails>([&] {
    take(validate_complete_exact_time_world(world, access, b));
    if (world.world_timeline->boundary_state != PeriodBoundaryState::open)
      fail("ExactPeriodOpeningRequired");
    const auto from =
        take(read_world_moment(world.common.tick(), *world.world_timeline, b));
    // 已开启的真实 Field 窗先完成玩家输入或本地结算；不能从 Complete
    // 入口绕过原宏观推进门。尚未到达下一宏观边界的 Running 由原门判定。
    if (world.field_battle) {
      const auto gate = take(read_field_world_advance_gate(
          *world.field_battle, world.common.state().metadata.player_actor_id,
          world.common.tick()));
      if (gate.reason != FieldWorldAdvanceReason::allowed)
        throw Failure{
            common_failure(gate.reason == FieldWorldAdvanceReason::player_input
                               ? "FieldBattlePlayerInputRequired"
                               : "FieldBattleLocalSettlementRequired",
                           gate.battle_id)
                .error};
    }
    const auto target = take(
        prepare_complete_exact_advance_target(world, access, requested, b));
    auto span = take(make_moment_span(from, target, b));
    const auto period = period_at(world, from, false);
    const bool boundary = take(compare_fractions(target.sub_half_day,
                                                 CanonicalFraction{}, b)) == 0;
    // The true submission stack owns this capture through the actual Common
    // seal. A local advance-only capture would destroy its death-tail proof.
    auto *actual = world.active_exact_write_context();
    if (!actual || !world.common.actor_condition_capture_identity())
      fail("CompleteAdvanceOuterCommandCaptureRequired");
    auto &writes = *actual;
    if (writes.actual_advance_span())
      fail("CompleteAdvanceSpanAlreadyBound");
    auto &result = writes.advance;
    {
      WorldExactAdvanceSpanScope actual_span(writes, span);
      take(provide_exact_intervals(access.runtime.phase_graph, world, span,
                                   writes));
      // 使用实际target作phase读窗；Common.tick整部最后才正规化，不让旧整数clock先触发半期结算。
      take(consume_exact_due_events(access.runtime.phase_graph, world, target,
                                    boundary, writes, technical, b,
                                    boundary ? &period : nullptr));
      if (boundary) {
        if (target.half_day_tick != period.through_tick)
          fail("ExactSliceCrossesPeriod");
        auto closed = take(close_exact_registered_period(
            access.runtime.phase_graph, world, period, writes));
        if (closed.empty())
          fail("MissingCompletePeriodOwner");
        // 原日历四腿与期间关闭均已消费；先反验同期间，再结束热候选寿命。
        if (writes.military_calendar) {
          take(validate_military_calendar_candidate_consumed(
              *writes.military_calendar, period, b));
          writes.military_calendar.reset();
        }
      }
    } // The selected-span witness ends before the actual Common publication.
    world.note_write_boundary();
    world.common.state().identity.tick = target.half_day_tick;
    world.world_timeline->sub_half_day = target.sub_half_day;
    if (boundary) {
      world.world_timeline->boundary_state = PeriodBoundaryState::ready;
      world.world_timeline->close_receipt_ref = period.period_id;
    }
    if (boundary) {
      take(consume_complete_growth_spatial_inputs_in_candidate(world, writes,
                                                               b));
      take(consume_complete_growth_quiet_inputs_in_candidate(world, writes, b));
      adopt_growth_condition_inputs(world, writes, b);
      validate_interaction_death_capture(world, writes);
      auto scope = take(world.enter_phase_read_context(
          "director_review", "director", {target, target}, target));
      take(review_complete_director_in_candidate(world, period,
                                                 access.director));
    }
    // Director 原命令可追加同刻真源；退其读票后沿原图消费，不重跑日历或复评。
    if (boundary)
      take(consume_exact_due_events(access.runtime.phase_graph, world, target,
                                    boundary, writes, technical, b, nullptr));
    // §1 原宏观算法与真实 Common/timeline 正规化完成后，才零时推进 NPC 本窗。
    // 仍在唯一未发布候选内；任何 Field 后果失败都由原提交口整体撤销。
    if (boundary && world.field_battle) {
      // 沿原军务计划产生真实任职/受令/成军/出征命令，不预写 Army 或接战。
      take(run_native_field_military_plans_in_candidate(world, writes));
      take(settle_native_field_npc_windows_in_candidate(world, writes));
      take(resume_native_never_admitted_field_boundaries_in_candidate(world, writes, false));
    }
    take(consume_complete_growth_spatial_inputs_in_candidate(world, writes, b));
    take(consume_complete_growth_quiet_inputs_in_candidate(world, writes, b));
    adopt_growth_condition_inputs(world, writes, b);
    take(
        validate_pending_growth_releases(world, writes.pending_study_releases));
    validate_interaction_death_capture(world, writes);
    // Field 宏观后计划/战场及上述 Growth 尾可追加真实 Common 头；同刻原图
    // 必须在发布前收齐 Social 等真实后果。此处不开期间日历、不重评 Director，
    // 不补写采集游标；原图逐源重读、反覆盖、回边拒绝与每调用预算仍生效。
    if (boundary && world.field_battle)
      take(consume_exact_due_events(access.runtime.phase_graph, world, target,
                                    boundary, writes, technical, b, nullptr));
    // This unpublished command still lacks its real Common result/history.
    // The single commit_tail binds original terminal releases and then calls
    // WorldBundle::validate before any pointer swap. The outer capture remains
    // live; result is only its existing transient AdvanceDetails output copy.
    // Applying published-state
    // validation here would demand a result that this writer must not invent.
    return result;
  });
}
} // namespace hanmo::resident
