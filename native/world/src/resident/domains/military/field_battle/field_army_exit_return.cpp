#include "field_army_exit_return.hpp"
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_exact_contact_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/actor_foundation/condition_application_history.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <initializer_list>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 按真实新增拥有字段预留，不以旧opening字节估计新condition/业务字符串。
void adopt_returning_state(CampaignRecord &campaign, FieldArmyExitPlan &plan,
    const SourceRef &ready, const SourceRef &continuation, const WorldMoment &now,
    CheckpointMemoryCharge &owner) {
  std::vector<StableId> keys;
  if (!campaign.field_order.empty()) {
    if (!campaign.return_garrison_id) keys.push_back("return_garrison_id");
    if (!campaign.return_wait_reason) keys.push_back("return_wait_reason");
    if (!campaign.objective_result) keys.push_back("objective_result");
  }
  owner.grow(0); // 真字段顺序和资源归属保持，普通 C++ 拷贝不先计量。
  if (!keys.empty()) {
    std::vector<StableId> order; order.reserve(campaign.field_order.size() + keys.size());
    for (const auto &key : campaign.field_order) order.push_back(checkpoint_fresh_string_copy(key));
    for (const auto &key : keys) order.push_back(checkpoint_fresh_string_copy(key));
    campaign.field_order.swap(order);
  }
  campaign.status = checkpoint_fresh_string_copy("returning");
  campaign.return_garrison_id = checkpoint_fresh_string_copy(plan.return_garrison_id);
  campaign.return_wait_reason = checkpoint_fresh_string_copy("");
  if (!campaign.objective_result)
    campaign.objective_result = CampaignObjectiveResult{checkpoint_fresh_string_copy("withdrawn"),
        now.half_day_tick, checkpoint_fresh_string_copy(ready.id), {}};
  plan.actual_continuation = SourceRef{checkpoint_fresh_string_copy(continuation.domain),
      checkpoint_fresh_string_copy(continuation.kind), checkpoint_fresh_string_copy(continuation.id)};
}
}
FieldArmyExitFiniteReturnRead::FieldArmyExitFiniteReturnRead(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a) : x_(x), catalog_(catalog), abilities_(a) {}
bool FieldArmyExitFiniteReturnRead::bound_to(const CommonStore &c, const MilitaryState &m,
    const SpatialCarrierState &s, const MovementExactState &e,
    const GroupMovementExecutionState &g) const noexcept {
  return &c == &x_.common && &m == &x_.military && &s == &x_.carriers &&
      &e == &x_.movement && &g == &x_.groups && abilities_.exact_contacts == &x_;
}
Outcome<MilitaryFiniteReturnOriginalRead> FieldArmyExitFiniteReturnRead::read_original_finite_return(
    const SourceRef &source) const {
  return guard<MilitaryFiniteReturnOriginalRead>([&] {
    const auto ready = take(read_field_army_exit_ready_source(x_, catalog_, abilities_, source));
    return MilitaryFiniteReturnOriginalRead{source, ready.plan->original_journey_opening,
        ready.plan->army_id, ready.plan->destination_settlement_id, ready.plan->return_garrison_id,
        ready.adopted_at, ready.actual_anchor, ready.named_participants};
  });
}
Outcome<MilitaryFiniteReturnOriginalRead> FieldArmyExitFiniteReturnRead::read_actual_finite_return_continuation(
    const SourceRef &source) const {
  return guard<MilitaryFiniteReturnOriginalRead>([&] {
    const auto actual = take(read_field_army_exit_continuation_source(x_, catalog_, abilities_, source));
    const auto &ready = actual.ready;
    return MilitaryFiniteReturnOriginalRead{*ready.plan->ready_source, ready.plan->original_journey_opening,
        ready.plan->army_id, ready.plan->destination_settlement_id, ready.plan->return_garrison_id,
        actual.opening ? actual.opening->occurred_at : actual.cancellation->occurred_at,
        ready.actual_anchor, ready.named_participants};
  });
}
FieldArmyExitOpeningAdoption::FieldArmyExitOpeningAdoption(MilitaryState &military,
    FieldBattleWorldState &field, const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const MilitaryFiniteReturnSourceRead &returns, SourceRef ready, CheckpointMemoryCharge &owner)
    : military_(military), field_(field), x_(x), catalog_(catalog), abilities_(a),
      returns_(returns), ready_(std::move(ready)), owner_(owner) {}
// 唯一新opening已由原S1分配；此处只采用真实ready有限返程，不调用CampaignDepart。
Status FieldArmyExitOpeningAdoption::adopt_original_order_in_candidate(CommonStore &common,
    SpatialCarrierState &carriers, MovementExactState &movement,
    GroupMovementExecutionState &groups, const StableId &journey_id) {
  return status([&] {
    if (!returns_.bound_to(common, military_, carriers, movement, groups) ||
        &field_ != &x_.field || &military_ != &x_.military || x_.original_finite_returns != &returns_)
      fail("MilitaryFiniteReturnSameWorldRequired", journey_id);
    const auto ready = take(read_field_army_exit_ready_source(x_, catalog_, abilities_, ready_));
    const auto now = take(x_.clock.read_current_moment());
    GroupMovementOpeningSource *opening = nullptr;
    for (auto &[id, row] : groups.openings.rows()) if (row.journey_id == journey_id) {
      if (opening) fail("MilitaryJourneyOpeningRequired", journey_id);
      opening = groups.openings.find(id);
    }
    auto *journey = carriers.journeys.find(journey_id);
    auto *prefix = journey ? movement.prefixes.find(journey->movement_prefix_ref) : nullptr;
    const auto *unit = prefix ? movement.units.find(prefix->unit_ref) : nullptr;
    const auto *carrier = journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
    if (!opening || opening->military_order || !journey || !unit || !carrier ||
        carrier->army_id != ready.plan->army_id || ready.plan->actual_continuation ||
        take(compare_world_moments(ready.adopted_at, now)) > 0 || !same_moment(opening->occurred_at, now) ||
        !std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) ||
        take(compare_fractions(unit->active_elapsed.value, CanonicalFraction{})) != 0 ||
        opening->named_participants != ready.named_participants ||
        !take(same_spatial_anchor(opening->original_anchor, ready.actual_anchor)))
      fail("MilitaryFiniteReturnActualOpeningRequired", journey_id);
    const SourceRef source{"travel", "group_movement_opening", opening->id};
    take(validate_field_army_exit_adoption_boundary(x_, catalog_, abilities_, ready_, source, now));
    const auto *old = take(read_military_group_order_acceptance(common, military_, x_.military_catalog,
        carriers, movement, groups, x_.conditions, ready.plan->original_journey_opening,
        now, x_.author, {}, &x_.formations, &returns_));
    const auto command = take(query_military_army_command_source_before_at(common, military_,
        ready.plan->army_id, opening->id, now, &x_.formations));
    const auto *actor = common.actor(command.commander_actor_id);
    const auto current = take(read_current_character_condition(x_.conditions, command.commander_actor_id));
    if (!actor || !actor->alive || !actor->available || !actor->action_id.empty() || !current.body.alive ||
        std::count(ready.named_participants.begin(), ready.named_participants.end(), command.commander_actor_id) != 1)
      fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
    CampaignRecord *campaign = nullptr;
    ArmyRecord *army = nullptr;
    for (auto &row : military_.campaigns) if (row.id == old->campaign_id) campaign = &row;
    for (auto &row : military_.armies) if (row.id == old->army_id) army = &row;
    if (!campaign || !army || army->status != "active" || campaign->army_id != army->id ||
        army->order_id != campaign->id || journey->payer_wallet_id != army->wallet_id ||
        campaign->status == "closed" || campaign->status == "defeated" || campaign->status == "awaiting_report")
      fail("MilitaryFiniteReturnOriginalCampaignRequired", journey_id);
    owner_.grow(0); // 原 opening/acceptance 仍完整普通复制，保原 job 检查。
    auto accepted = *old;
    accepted.kind = MilitaryMarchOrderKind::finite_return;
    accepted.commander_actor_id = command.commander_actor_id;
    accepted.command_source = command.command_source;
    accepted.commander_condition_head = current.head;
    accepted.accepted_at = now;
    accepted.destination_settlement_id = ready.plan->destination_settlement_id;
    accepted.return_garrison_id = ready.plan->return_garrison_id;
    accepted.original_return_cause = ready_;
    groups.openings.find(opening->id)->military_order = std::move(accepted);
    journey->expense_authority = MilitaryMarchExpenseAuthority{source, army->id, campaign->id, army->wallet_id};
    // 原campaign只沿实际ready进入安全返回，不先卸装/关闭/结算奖励。
    auto plan = field_.army_exit_plans.find(ready.plan->fact_id);
    if (plan == field_.army_exit_plans.end()) fail("FieldArmyExitReadySourceRequired", ready_.id);
    adopt_returning_state(*campaign, plan->second, ready_, source, now, owner_);
    take(read_military_group_order_acceptance(common, military_, x_.military_catalog,
        carriers, movement, groups, x_.conditions, source, now, x_.author, {}, &x_.formations, &returns_));
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 同目标聚落的真实无世界路程分支；仍保整军公物，不启动未选卸装动作。
Status adopt_field_army_exit_at_destination_in_candidate(CommonStore &common,
    MilitaryState &military, FieldBattleWorldState &field,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source,
    const SourceRef &cancel_source, CheckpointMemoryCharge &owner) {
  return status([&] {
    if (&common != &x.common || &military != &x.military || &field != &x.field ||
        !x.original_finite_returns || !x.original_finite_returns->bound_to(common, military, x.carriers, x.movement, x.groups))
      fail("MilitaryFiniteReturnSameWorldRequired", source.id);
    const auto ready = take(read_field_army_exit_ready_source(x, catalog, a, source));
    const auto now = take(x.clock.read_current_moment());
    const auto *anchor = std::get_if<OriginalMovementAnchor>(&ready.actual_anchor);
    const auto *location = anchor ? std::get_if<SettlementLocation>(&anchor->location) : nullptr;
    const auto *cancel = cancel_source.domain == "travel" && cancel_source.kind == "group_movement_control" ?
        x.groups.controls.find(cancel_source.id) : nullptr;
    const auto key = spatial_carrier_key(SpatialCarrierRef{ArmyCarrierRef{ready.plan->army_id}});
    const auto *binding = x.movement.spatial_bindings.find(key);
    const auto *stable = binding ? std::get_if<StableMovementWriter>(&binding->writer) : nullptr;
    if (!location || location->settlement_id != ready.plan->destination_settlement_id ||
        take(compare_world_moments(ready.adopted_at, now)) > 0 || ready.plan->actual_continuation ||
        !cancel || cancel->operation != GroupControlOperation::cancel ||
        !same_source(cancel->authority_source, source) || !same_moment(cancel->occurred_at, now) ||
        cancel->journey_id != ready.original_journey->journey_id ||
        !stable || !same_source(binding->binding_source, source) ||
        !take(same_spatial_anchor(stable->anchor, ready.actual_anchor)))
      fail("FieldArmyExitActualDestinationRequired", source.id);
    take(validate_field_army_exit_adoption_boundary(x, catalog, a, source, cancel_source, now));
    const auto *old = take(read_military_group_order_acceptance(common, military, x.military_catalog,
        x.carriers, x.movement, x.groups, x.conditions, ready.plan->original_journey_opening,
        now, x.author, {}, &x.formations, x.original_finite_returns));
    const auto command = take(query_military_army_command_source_before_at(common, military,
        ready.plan->army_id, cancel_source.id, now, &x.formations));
    const auto *actor = common.actor(command.commander_actor_id);
    if (!actor || !actor->alive || !actor->available || !actor->action_id.empty() ||
        !take(read_current_character_condition(x.conditions, command.commander_actor_id)).body.alive ||
        std::count(ready.named_participants.begin(), ready.named_participants.end(), command.commander_actor_id) != 1)
      fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
    CampaignRecord *campaign = nullptr;
    for (auto &row : military.campaigns) if (row.id == old->campaign_id) campaign = &row;
    if (!campaign || campaign->army_id != ready.plan->army_id ||
        campaign->status == "closed" || campaign->status == "defeated" || campaign->status == "awaiting_report")
      fail("MilitaryFiniteReturnOriginalCampaignRequired", source.id);
    auto plan = field.army_exit_plans.find(ready.plan->fact_id);
    if (plan == field.army_exit_plans.end()) fail("FieldArmyExitReadySourceRequired", source.id);
    adopt_returning_state(*campaign, plan->second, source, cancel_source, now, owner);
    take(read_field_army_exit_continuation_source(x, catalog, a, cancel_source));
  });
}
// 此读口只反读有限原链；不调用全group/engagement反覆盖，避免采用终点倒入本源。
Outcome<FieldArmyExitContinuationRead> read_field_army_exit_continuation_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source, ExactArithmeticBudget b) {
  return guard<FieldArmyExitContinuationRead>([&] {
    const bool at_destination = source.domain == "travel" && source.kind == "group_movement_control";
    if (source.domain != "travel" || (source.kind != "group_movement_opening" && !at_destination) || !x.original_finite_returns)
      fail("FieldArmyExitReturnContinuationSourceRequired", source.id);
    FieldArmyExitContinuationRead out;
    const GroupMovementControlSource *destination_cancel = at_destination ? x.groups.controls.find(source.id) : nullptr;
    out.opening = at_destination ? nullptr : x.groups.openings.find(source.id);
    const auto *order = out.opening && out.opening->military_order ? &*out.opening->military_order : nullptr;
    SourceRef cause;
    if (at_destination) {
      if (!destination_cancel || destination_cancel->operation != GroupControlOperation::cancel)
        fail("FieldArmyExitReturnContinuationSourceRequired", source.id);
      cause = destination_cancel->authority_source;
    } else {
      if (!order || order->kind != MilitaryMarchOrderKind::finite_return || !order->original_return_cause)
        fail("FieldArmyExitReturnContinuationSourceRequired", source.id);
      cause = *order->original_return_cause;
    }
    if (take(read_actual_common_fact_position(x.common, cause)) >=
        take(read_actual_common_fact_position(x.common, source)))
      fail("FieldArmyExitReturnContinuationOrderRequired", source.id);
    out.ready = take(read_field_army_exit_ready_source(x, catalog, a, cause, b));
    if (!out.ready.plan->actual_continuation || !same_source(*out.ready.plan->actual_continuation, source))
      fail("FieldArmyExitReturnContinuationBindingRequired", source.id);
    const auto actual_at = at_destination ? destination_cancel->occurred_at : out.opening->occurred_at;
    take(validate_field_army_exit_adoption_boundary(x, catalog, a, cause, source, actual_at));
    const auto ready_position = take(read_actual_common_fact_position(x.common, cause));
    const auto opening_position = take(read_actual_common_fact_position(x.common, source));
    for (const auto &[id, control] : x.groups.controls.rows())
      if (same_source(control.authority_source, cause)) {
        if (out.cancellation || control.journey_id != out.ready.original_journey->journey_id ||
            control.operation != GroupControlOperation::cancel || !same_moment(control.occurred_at, actual_at, b) ||
            !take(same_spatial_anchor(control.actual_anchor, out.ready.original_close->actual_anchor, b)))
          fail("FieldArmyExitReturnOriginalCancellationRequired", id);
        const auto position = take(read_actual_common_fact_position(x.common,
            SourceRef{"travel", "group_movement_control", id}));
        if (position <= ready_position || (at_destination ? position != opening_position : position >= opening_position))
          fail("FieldArmyExitReturnContinuationOrderRequired", id);
        out.cancellation = &control;
      }
    if (!out.cancellation) fail("FieldArmyExitReturnOriginalCancellationRequired", source.id);
    const auto terminal = take(read_actual_group_journey_terminal(x.common, x.carriers,
        x.movement, x.groups, out.ready.original_journey->journey_id, b));
    if (terminal.terminal.outcome != GroupJourneyTerminalOutcome::cancelled ||
        !same_source(terminal.terminal.actual_terminal_source,
            {"travel", "group_movement_control", out.cancellation->id}))
      fail("FieldArmyExitReturnOriginalCancellationRequired", source.id);
    for (const auto &[id, opening] : x.groups.openings.rows())
      if ((at_destination || id != source.id) && opening.military_order && opening.military_order->original_return_cause &&
          same_source(*opening.military_order->original_return_cause, cause))
        fail("FieldArmyExitReturnContinuationAmbiguous", source.id);
    // 实际采用真头前的原指挥/身体/可用来源；不借今日状态替历史准入背书。
    const auto command = take(query_military_army_command_source_before_at(x.common, x.military,
        out.ready.plan->army_id, source.id, take(x.clock.read_current_moment()), &x.formations));
    const auto body = take(read_character_condition_before_application_fact(x.conditions,
        command.commander_actor_id, source, actual_at));
    const auto availability = take(read_character_availability_before_fact_provenance_v2(x.conditions,
        command.commander_actor_id, source, actual_at, x.clock));
    if (!body.body.alive || !availability.present || !availability.alive || !availability.available ||
        std::count(out.ready.named_participants.begin(), out.ready.named_participants.end(), command.commander_actor_id) != 1)
      fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
    if (at_destination) {
      const auto *anchor = std::get_if<OriginalMovementAnchor>(&out.ready.actual_anchor);
      const auto *settlement = anchor ? std::get_if<SettlementLocation>(&anchor->location) : nullptr;
      if (!anchor || !settlement || settlement->settlement_id != out.ready.plan->destination_settlement_id ||
          out.cancellation != destination_cancel || !same_source(anchor->actual_source, cause))
        fail("FieldArmyExitActualDestinationRequired", source.id);
      out.already_at_destination = true;
      return out;
    }
    take(read_military_group_order_acceptance(x.common, x.military, x.military_catalog,
        x.carriers, x.movement, x.groups, x.conditions, source, take(x.clock.read_current_moment()),
        x.author, b, &x.formations, x.original_finite_returns));
    out.journey = x.carriers.journeys.find(out.opening->journey_id);
    if (!out.journey || order->army_id != out.ready.plan->army_id ||
        out.opening->named_participants != out.ready.named_participants ||
        !same_moment(out.opening->occurred_at, actual_at, b) ||
        !take(same_spatial_anchor(out.opening->original_anchor, out.ready.actual_anchor, b)))
      fail("FieldArmyExitReturnContinuationSourceMismatch", source.id);
    return out;
  });
}
} // namespace hanmo::resident
