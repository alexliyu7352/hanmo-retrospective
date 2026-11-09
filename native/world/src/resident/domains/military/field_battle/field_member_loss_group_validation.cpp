#include "field_exit_placement.hpp"
#include "field_member_loss_group_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
Status OriginalFieldBattleExitPlacementRead::validate_actual_group_member_loss(
    const GroupMemberLossHandoffSource &loss) const {
  const auto &x = context_;
  auto fail = [&](const char *code) { return common_failure(code, loss.id); };
  if (x.groups.member_loss_handoffs.find(loss.id) != &loss)
    return fail("FieldGroupActualLossRowRequired");
  auto origin = read_field_member_loss_group_origin(x, loss.actual_casualty,
                                                    loss.actor_id, budget_);
  if (!origin.ok)
    return {false, {}, origin.error};
  if (origin.value.journey->journey_id != loss.journey_id ||
      !same_source_ref(*origin.value.original_contact, loss.original_contact) ||
      origin.value.casualty_ordinal != loss.casualty_ordinal)
    return fail("FieldGroupLossOriginalMemberMismatch");
  auto casualty_head =
      read_actual_common_fact_position(x.common, loss.actual_casualty);
  auto loss_head = read_actual_common_fact_position(
      x.common,
      SourceRefBuildView{"travel", "group_member_loss_handoff", loss.id});
  if (!casualty_head.ok)
    return {false, {}, casualty_head.error};
  if (!loss_head.ok)
    return {false, {}, loss_head.error};
  if (casualty_head.value >= loss_head.value)
    return fail("FieldGroupLossActualAdoptionOrderMismatch");
  auto actual = compare_world_moments(
      *origin.value.applied_at, loss.occurred_at, budget_);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (actual.value)
    return fail("FieldGroupLossOriginalMomentMismatch");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (const auto *dead =
          std::get_if<GroupMemberDeathApplication>(&loss.actual_body)) {
    if (origin.value.actor_change->status != "dead" || !x.original_deaths ||
        !x.original_life || &x.original_life->military != &x.military ||
        x.original_life->field_battles != &x.field ||
        !x.original_life->field_application_clock ||
        !x.original_life->field_application_clock->bound_to(
            x.common, x.military, x.field))
      return fail("FieldGroupLossOriginalLifeOwnerRequired");
    auto life_now =
        x.original_life->field_application_clock->read_current_moment();
    if (!life_now.ok)
      return {false, {}, life_now.error};
    auto same_now = compare_world_moments(life_now.value, now.value, budget_);
    if (!same_now.ok)
      return {false, {}, same_now.error};
    if (same_now.value)
      return fail("FieldGroupLossLifeClockMismatch");
    auto death = read_verified_death4_record(
        x.common, *x.original_deaths, *x.original_life, dead->actual_death,
        now.value, MilitaryCasualtyHistoryOrder::actual_common_prefix);
    if (!death.ok)
      return {false, {}, death.error};
    const auto *cause =
        death.value->precise
            ? std::get_if<MilitaryDeathCause4>(&death.value->precise->cause)
            : nullptr;
    if (!cause || death.value->actor_id != loss.actor_id ||
        cause->actor_id != loss.actor_id ||
        cause->casualty_fact_id != loss.actual_casualty.id ||
        cause->battle_id != origin.value.segment.session->battle_id)
      return fail("FieldGroupLossOriginalDeathCauseMismatch");
    auto effective = compare_world_moments(death.value->precise->effective_at,
                                           loss.occurred_at, budget_);
    auto caused =
        compare_world_moments(cause->occurrence_at, loss.occurred_at, budget_);
    if (!effective.ok)
      return {false, {}, effective.error};
    if (!caused.ok)
      return {false, {}, caused.error};
    if (effective.value || caused.value)
      return fail("FieldGroupLossOriginalDeathMomentMismatch");
  } else if (std::holds_alternative<GroupMemberCaptivityApplication>(
                 loss.actual_body) ||
             std::holds_alternative<GroupMemberDesertionApplication>(
                 loss.actual_body)) {
    const bool captured =
        std::holds_alternative<GroupMemberCaptivityApplication>(
            loss.actual_body);
    const auto &capacity =
        captured ? std::get<GroupMemberCaptivityApplication>(loss.actual_body)
                       .actual_capacity_transition
                 : std::get<GroupMemberDesertionApplication>(loss.actual_body)
                       .actual_capacity_transition;
    if (origin.value.actor_change->status !=
            (captured ? "captured" : "deserted") ||
        !x.original_body || &x.original_body->common != &x.common ||
        x.original_body->military != &x.military ||
        x.original_body->injury_field != &x.field ||
        !x.original_body->injury_field_clock ||
        !x.original_body->injury_field_clock->bound_to(x.common, x.military,
                                                       x.field))
      return fail("FieldGroupLossOriginalConditionOwnerRequired");
    auto body_now = x.original_body->injury_field_clock->read_current_moment();
    if (!body_now.ok)
      return {false, {}, body_now.error};
    auto same_now = compare_world_moments(body_now.value, now.value, budget_);
    auto same_body =
        compare_world_moments(x.original_body->actual_now, now.value, budget_);
    if (!same_now.ok)
      return {false, {}, same_now.error};
    if (!same_body.ok)
      return {false, {}, same_body.error};
    if (same_now.value || same_body.value)
      return fail("FieldGroupLossConditionClockMismatch");
    // 成员损失采用引用的是原 Field casualty 行，不能让之后的 custody release
    // 经全 actor 后继重放反过来成为本次早期交接的来源。
    auto row = read_original_field_availability_transition_source(
        *x.original_body, capacity, 2);
    if (!row.ok)
      return {false, {}, row.error};
    if (row.value->actor_id != loss.actor_id || row.value->to ||
        row.value->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
      return fail("FieldGroupLossOriginalAvailabilityMismatch");
    auto cause =
        read_original_availability_cause_position(*x.original_body, *row.value);
    if (!cause.ok)
      return {false, {}, cause.error};
    if (!cause.value.source || cause.value.schema != 1 ||
        cause.value.ordinal != loss.casualty_ordinal ||
        !same_source_ref(*cause.value.source, loss.actual_casualty))
      return fail("FieldGroupLossOriginalAvailabilityCauseMismatch");
    auto effective = compare_world_moments(row.value->effective_at.exact_moment,
                                           loss.occurred_at, budget_);
    if (!effective.ok)
      return {false, {}, effective.error};
    if (effective.value)
      return fail("FieldGroupLossOriginalAvailabilityMomentMismatch");
    if (captured) {
      auto original = read_field_segment_captivity_source(
          x.common, x.military, x.field, loss.actual_casualty, loss.actor_id,
          x.clock);
      if (!original.ok)
        return {false, {}, original.error};
      if (original.value.source_ordinal != loss.casualty_ordinal)
        return fail("FieldGroupLossOriginalCaptivityOrdinalMismatch");
    } else {
      if (origin.value.ordinary_loss) {
        const auto &actual = *origin.value.ordinary_loss;
        auto step_head = read_actual_common_fact_position(x.common,
            SourceRefBuildView{"military", "field_army_exit_step", actual.step->fact_id});
        if (!step_head.ok) return {false, {}, step_head.error};
        if (step_head.value >= casualty_head.value || casualty_head.value >= loss_head.value)
          return fail("FieldArmyExitLossActualAdoptionOrderMismatch");
      } else {
      auto exit =
          read_field_member_desertion_exit_source(x, origin.value, budget_);
      if (!exit.ok)
        return {false, {}, exit.error};
      auto exit_head = read_actual_common_fact_position(
          x.common, SourceRefBuildView{"military", "field_battle_exit",
                                       exit.value->fact_id});
      if (!exit_head.ok)
        return {false, {}, exit_head.error};
      if (exit_head.value >= loss_head.value)
        return fail("FieldGroupLossActualDesertionExitOrderMismatch");
      }
    }
  } else
    return fail("FieldGroupLossBodyKindUnsupported");
  // This is the original business/source half only. The S1 actual writer and
  // complete spatial history independently verify this handoff's real binding;
  // an isolated row is never a BoundSpatialLocationWrite or publication proof.
  return common_success();
}
} // namespace hanmo::resident
