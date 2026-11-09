#include "field_army_group_origin.hpp"
#include "field_army_exit_placement.hpp"
#include "field_exit_placement.hpp"
#include "field_member_loss_group_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {
Outcome<FieldActorMemberLossPlacementRead>
OriginalFieldBattleExitPlacementRead::read_field_member_loss_placement(
    const MomentOwnerSource &source, const StableId &actor) const {
  const auto &x = context_;
  auto fail =
      [&](const char *code) -> Outcome<FieldActorMemberLossPlacementRead> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (source.source_schema != 1 ||
      !is_field_member_loss_placement_source(source.source_ref))
    return fail("FieldMemberLossPlacementSchemaRequired");
  const auto *loss = x.groups.member_loss_handoffs.find(source.source_ref.id);
  if (!loss || loss->actor_id != actor)
    return fail("FieldMemberLossPlacementOriginalRowRequired");
  auto all =
      validate_group_member_loss_handoffs(x.common, x.movement, x.groups, this);
  if (!all.ok)
    return {false, {}, all.error};
  auto origin = read_field_member_loss_group_origin(x, loss->actual_casualty,
                                                    actor, budget_);
  if (!origin.ok)
    return {false, {}, origin.error};
  std::optional<CheckpointMemoryCharge> deserted_memory;
  std::optional<MovementAnchor> deserted_anchor;
  const MovementAnchor *anchor = &origin.value.closed->actual_anchor;
  const StableId *carried = nullptr;
  if (std::holds_alternative<GroupMemberCaptivityApplication>(
          loss->actual_body)) {
    auto captivity = read_field_segment_captivity_source(
        x.common, x.military, x.field, loss->actual_casualty, actor, x.clock);
    if (!captivity.ok)
      return {false, {}, captivity.error};
    if (!captivity.value.captor_binding)
      return fail("FieldMemberLossOriginalCaptorRequired");
    auto captor = read_field_original_army_group_origin(
        x, *captivity.value.captor_binding, budget_);
    if (!captor.ok)
      return {false, {}, captor.error};
    auto entered = read_actual_common_fact_position(
        x.common, *captor.value.original_contact);
    auto casualty =
        read_actual_common_fact_position(x.common, loss->actual_casualty);
    if (!entered.ok)
      return {false, {}, entered.error};
    if (!casualty.ok)
      return {false, {}, casualty.error};
    auto time = compare_world_moments(captor.value.closed->occurred_at,
                                      loss->occurred_at, budget_);
    if (!time.ok)
      return {false, {}, time.error};
    if (entered.value >= casualty.value || time.value > 0)
      return fail("FieldMemberLossCaptorOriginalOrderMismatch");
    carried = &captivity.value.actor_change->captor_army_id;
    anchor = &captor.value.closed->actual_anchor;
  } else if (std::holds_alternative<GroupMemberDesertionApplication>(
                 loss->actual_body)) {
    if (origin.value.ordinary_loss) {
      if (!x.original_abilities || !x.exact_contacts)
        return fail("FieldArmyExitLossOriginalAbilitiesRequired");
      const auto &step = *origin.value.ordinary_loss->step;
      auto projected = read_field_army_exit_ordinary_anchor(*x.exact_contacts, x.catalog,
          *x.original_abilities, {"military", "field_army_exit_step", step.fact_id}, actor, budget_);
      if (!projected.ok) return {false, {}, projected.error};
      deserted_anchor = std::move(projected.value);
    } else {
    auto exit =
        read_field_member_desertion_exit_source(x, origin.value, budget_);
    if (!exit.ok)
      return {false, {}, exit.error};
    const auto &e = *exit.value;


    // 沿原两种位置的实际字符串预留；不请求未导出的私有 variant 实例。
    if (const auto *road = std::get_if<OnRouteLocation>(&e.location)) {


    } else {
      const auto &town = std::get<SettlementLocation>(e.location);


    }

    deserted_memory.emplace(CheckpointMemoryCharge(0));
    const SourceRef exit_source{checkpoint_fresh_text_copy("military"),
                                checkpoint_fresh_text_copy("field_battle_exit"),
                                checkpoint_fresh_string_copy(e.fact_id)};
    if (const auto *road = std::get_if<OnRouteLocation>(&e.location)) {
      if (!e.exact_route_progress)
        return fail("FieldDesertionOriginalProgressRequired");
      deserted_anchor = RoadFractionalAnchor{
          road->edge_id, road->origin, *e.exact_route_progress, exit_source};
    } else if (const auto *town =
                   std::get_if<SettlementLocation>(&e.location)) {
      if (e.exact_route_progress)
        return fail("FieldDesertionUnexpectedProgress");
      deserted_anchor = OriginalMovementAnchor{*town, exit_source};
    } else
      return fail("FieldDesertionOriginalPlacementRequired");
    }
    anchor = &*deserted_anchor;
  } else if (!std::holds_alternative<GroupMemberDeathApplication>(
                 loss->actual_body))
    return fail("FieldMemberLossPlacementKindUnsupported");
  CheckpointVector<ActualSpatialSourceBuildView> heads;
  heads.push_back(
      {{source.source_ref.domain, source.source_ref.kind, source.source_ref.id},
       1});
  return {true,
          {loss,
           ActualActorSpatialWriteRead{ActualActorSpatialWriteReadBuildView{
               actor, loss->occurred_at, *anchor, heads}},
           carried},
          {}};
}
Outcome<CheckpointVector<FieldActorMemberLossPlacementRead>>
OriginalFieldBattleExitPlacementRead::read_field_member_loss_placement_history(
    const StableId &actor) const {
  CheckpointVector<FieldActorMemberLossPlacementRead> out;
  for (const auto &row : context_.groups.member_loss_handoffs.rows()) {
    if (row.second.actor_id != actor)
      continue;


    auto memory = CheckpointMemoryCharge(0);
    const MomentOwnerSource source{
        {checkpoint_fresh_text_copy("travel"),
         checkpoint_fresh_text_copy("group_member_loss_handoff"),
         checkpoint_fresh_string_copy(row.first)},
        1};
    auto actual = read_field_member_loss_placement(source, actor);
    if (!actual.ok)
      return {false, {}, actual.error};
    out.push_back(std::move(actual.value));
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
