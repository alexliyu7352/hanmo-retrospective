#include "native_field_army_exit_return.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_scope.hpp"
#include "native_military_training_period.hpp"
#include "resident/domains/military/field_battle/field_army_exit_return.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/travel/field_army_exit_group_adoption.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure { RuleError error; };
template <class T> T take(Outcome<T> result) {
  if (!result.ok) throw Failure{std::move(result.error)};
  return std::move(result.value);
}
void take(Status result) {
  if (!result.ok) throw Failure{std::move(result.error)};
}
void require(bool value, const char *code, const StableId &id = {}) {
  if (!value) throw Failure{common_failure(code, id).error};
}
} // namespace
// 编队、钱包、军令与路线取原开程；位置取实际ready，采用时刻取经原期末证明的当前M。
Status adopt_native_field_army_exit_ready_in_candidate(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    NativeFieldGroupWorld &group, const SourceRef &source,
    ExactPhaseWriteContext &writes) {
  try {
    require(world.catalog && world.catalog->field_group_passage && world.field_battle &&
        world.spatial_carriers && world.movement_exact && world.group_movement_executions &&
        world.read_stamps && world.active_exact_write_context() == &writes &&
        world.common.actor_location_capture_identity() && group.bound_to(world) &&
        owner.reads().original_core && &owner.reads().original_core->characters.early.world == &world &&
        &owner.reads().original_core->characters.early.complete == &owner,
        "FieldArmyExitActualCandidateRequired", source.id);
    take(group.validate_bindings());
    auto &scope = group.field();
    const auto *x = scope.exact_contacts();
    const auto *finite = scope.finite_returns();
    const auto &early = owner.reads().original_core->characters.early;
    require(x && &x->formations == early.formations &&
        &x->clock == early.field_application_clock && x->periods == early.periods &&
        scope.placements() && finite &&
        finite->bound_to(world.common, world.military, *world.spatial_carriers,
            *world.movement_exact, *world.group_movement_executions),
        "FieldArmyExitActualOriginalSourcesRequired", source.id);
    const auto ready = take(read_field_army_exit_ready_source(*x,
        *world.catalog->field_battle, scope.abilities(), source));
    const auto at = take(world.actual_phase_moment());
    require(ready.plan && ready.original_journey && ready.original_opening &&
        ready.original_opening->military_order && !ready.plan->actual_continuation &&
        take(compare_world_moments(ready.adopted_at, at)) <= 0 &&
        at.half_day_tick == world.common.tick() &&
        take(compare_fractions(at.sub_half_day, CanonicalFraction{})) == 0,
        "FieldArmyExitActualClosingRequired", source.id);
    // 等待不是已采用成功；driver先辨真实Unavailable，直调者仍明确拒绝。
    require(!take(read_field_army_exit_return_wait(*x, *world.catalog->field_battle,
        scope.abilities(), source)), "FieldArmyExitCommanderUnavailable", source.id);
    take(validate_field_army_exit_current_adoption_window(*x,
        *world.catalog->field_battle, scope.abilities(), source));
    const auto &old = *ready.original_journey;
    const auto &order = *ready.original_opening->military_order;
    const auto manifest = take(read_transport_manifest(world.common,
        *world.spatial_carriers, *world.movement_exact, group.group().carriers,
        ArmyCarrierRef{ready.plan->army_id}));
    ArmyGroupJourneyOpeningInput request;
    request.carrier = ArmyCarrierRef{ready.plan->army_id};
    request.expected_composition_revision = manifest.composition_revision;
    request.issuing_actor_id = order.issuing_actor_id;
    request.campaign_id = order.campaign_id;
    request.organization_id = old.sponsor.id;
    request.payer_wallet_id = old.payer_wallet_id;
    request.destination = ready.plan->destination_settlement_id;
    request.named_march_participants = ready.named_participants;
    request.route_profile = old.route_profile;
    FieldArmyExitOpeningAdoption adoption(world.military, *world.field_battle,
        *x, *world.catalog->field_battle, scope.abilities(), *finite, source,
        world.checkpoint_copy_memory);
    ReadStampWriteBoundary boundary(*world.read_stamps);
    const auto actual = take(adopt_field_army_exit_ready_in_candidate(
        world.common, world.military, *world.spatial_carriers, *world.movement_exact,
        *world.group_movement_executions, *world.field_battle, group.group(), *x,
        *world.catalog->field_battle, scope.abilities(), *scope.placements(), source,
        request, adoption, writes.spatial_location_writes, world.checkpoint_copy_memory));
    // 原typed冷reader验证真实采用，不根据状态字串或非空列表冒成功。
    const auto adopted = take(read_field_army_exit_continuation_source(*x,
        *world.catalog->field_battle, scope.abilities(), actual));
    const auto adopted_at = adopted.opening ? adopted.opening->occurred_at :
        adopted.cancellation->occurred_at;
    require(adopted.ready.plan == ready.plan &&
        take(compare_world_moments(adopted_at, at)) == 0 &&
        take(compare_world_moments(at, take(world.actual_phase_moment()))) == 0,
        "FieldArmyExitActualContinuationMismatch", source.id);
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldArmyExitConsumerResourceExhausted", {}, ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldArmyExitConsumerResourceExhausted", {}, ErrorKind::internal);
  }
}
} // namespace hanmo::resident
