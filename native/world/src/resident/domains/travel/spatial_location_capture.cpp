#include "spatial_location_capture.hpp"
#include "local_move_source.hpp"
#include "field_army_exit_stable_capture.hpp"
#include "../military/named_military_custody_history_model.hpp"
#include "../military/named_military_custody_placement.hpp"
#include "birth_placement.hpp"
#include "field_battle_exit_placement.hpp"
#include "group_member_loss_handoff.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "group_transport_registration.hpp"
#include "military_action_placement.hpp"
#include "movement_control.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <cstdio>
#include <set>
#include <type_traits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
CheckpointMemoryCharge
reserve_bound_write(const BoundSpatialLocationWriteBuildView &input) {


  return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge
reserve_spatial_write_read(const ActualActorSpatialWriteReadBuildView &input) {


  return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge
reserve_spatial_write_read(const ActualActorSpatialWriteRead &input) {


  return CheckpointMemoryCharge(0);
}
BoundSpatialLocationWriteBuildView
bound_write_view(const BoundSpatialLocationWrite &value) {
  return {value.actor_id,
          {value.actual_source.domain, value.actual_source.kind,
           value.actual_source.id},
          value.source_schema,
          value.actual_anchor,
          value.capture_identity,
          value.capture_sequence,
          value.prefix_witness ? &*value.prefix_witness : nullptr,
          value.write_identity};
}
} // namespace
BoundSpatialLocationWrite::BoundSpatialLocationWrite(
    const BoundSpatialLocationWriteBuildView &input)
    : copied_fields(reserve_bound_write(input)), actor_id(input.actor_id),
      actual_source{StableId(input.actual_source.domain),
                    StableId(input.actual_source.kind),
                    StableId(input.actual_source.id)},
      source_schema(input.source_schema), actual_anchor(input.actual_anchor),
      capture_identity(input.capture_identity),
      capture_sequence(input.capture_sequence),
      write_identity(input.write_identity) {
  // Direct emplacement makes exactly the planned fresh witness. Its exact
  // limbs use their own original allocator; the lasting payload lease is first.
  if (input.prefix_witness)
    prefix_witness.emplace(*input.prefix_witness);
}
BoundSpatialLocationWrite::BoundSpatialLocationWrite(
    const BoundSpatialLocationWrite &value)
    : BoundSpatialLocationWrite(bound_write_view(value)) {}
BoundSpatialLocationWrite::BoundSpatialLocationWrite(
    BoundSpatialLocationWrite &&value)
    : BoundSpatialLocationWrite(
          static_cast<const BoundSpatialLocationWrite &>(value)) {}
ActualActorSpatialWriteRead::ActualActorSpatialWriteRead(
    const ActualActorSpatialWriteReadBuildView &input)
    : copied_fields(reserve_spatial_write_read(input)),
      actor_id(input.actor_id), occurred_at(input.occurred_at),
      actual_anchor(input.actual_anchor) {
  static_assert(
      std::is_nothrow_move_constructible_v<ActualSpatialSourceRef>,
      "the planned actual source must transfer without another payload copy");
  immutable_source_head.reserve(input.immutable_source_head.size());
  for (const auto &head : input.immutable_source_head)
    immutable_source_head.push_back(ActualSpatialSourceRef{
        {StableId(head.source.domain), StableId(head.source.kind),
         StableId(head.source.id)},
        head.schema});
}
ActualActorSpatialWriteRead::ActualActorSpatialWriteRead(
    const ActualActorSpatialWriteRead &value)
    : copied_fields(reserve_spatial_write_read(value)),
      actor_id(value.actor_id), occurred_at(value.occurred_at),
      actual_anchor(value.actual_anchor),
      immutable_source_head(value.immutable_source_head) {}
ActualActorSpatialWriteRead::ActualActorSpatialWriteRead(
    ActualActorSpatialWriteRead &&value)
    : ActualActorSpatialWriteRead(
          static_cast<const ActualActorSpatialWriteRead &>(value)) {}
namespace {
bool same_location(const Location &a, const Location &c) {
  if (a.index() != c.index())
    return false;
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto &y = std::get<T>(c);
        if constexpr (std::is_same_v<T, SettlementLocation>)
          return x.settlement_id == y.settlement_id && x.place_id == y.place_id;
        else if constexpr (std::is_same_v<T, WildernessLocation>)
          return x.map_id == y.map_id && x.origin == y.origin &&
                 x.point.x == y.point.x && x.point.y == y.point.y;
        else if constexpr (std::is_same_v<T, OnRouteLocation>)
          return x.edge_id == y.edge_id && x.origin == y.origin &&
                 x.progress == y.progress;
        else if constexpr (std::is_same_v<T, FollowingLocation>)
          return x.party_id == y.party_id;
        else if constexpr (std::is_same_v<T, InArmyLocation>)
          return x.army_id == y.army_id;
        else
          return x.carrier_kind == y.carrier_kind &&
                 x.carrier_id == y.carrier_id;
      },
      a);
}
bool same_anchor(const MovementAnchor &a, const MovementAnchor &c,
                 ExactArithmeticBudget b) {
  if (a.index() != c.index())
    return false;
  if (const auto *x = std::get_if<OriginalMovementAnchor>(&a))
    return same_location(x->location,
                         std::get<OriginalMovementAnchor>(c).location) &&
           same_source(x->actual_source,
                       std::get<OriginalMovementAnchor>(c).actual_source);
  if (const auto *x = std::get_if<RoadFractionalAnchor>(&a)) {
    const auto &y = std::get<RoadFractionalAnchor>(c);
    return x->edge_id == y.edge_id && x->origin == y.origin &&
           compare(x->progress, y.progress, b) == 0 &&
           same_source(x->actual_source, y.actual_source);
  }
  const auto &x = std::get<TerrainContinuousAnchor>(a);
  const auto &y = std::get<TerrainContinuousAnchor>(c);
  return x.map_id == y.map_id && x.origin == y.origin && x.x_bits == y.x_bits &&
         x.y_bits == y.y_bits &&
         same_source(x.actual_prefix_close_source,
                     y.actual_prefix_close_source) &&
         x.geometry_profile_ref == y.geometry_profile_ref;
}
// 只接受同根原退出正文；输出头和锚必须仍指同一个 schema1 实际事实。
ActualActorSpatialWriteRead
read_field_exit(const CommonStore &common, const MovementExactState &movement,
                const BoundSpatialLocationWrite &bound,
                const FieldBattleExitPlacementRead *original,
                ExactArithmeticBudget budget) {
  if (!original || !original->bound_to(common, movement) ||
      bound.source_schema != 1 ||
      !is_field_battle_exit_placement_source(bound.actual_source))
    fail("FieldExitPlacementOriginalReadRequired", bound.actor_id);
  const auto *fact = common.state().facts.find(bound.actual_source.id);
  if (!fact || fact->domain != bound.actual_source.domain ||
      fact->kind != bound.actual_source.kind)
    fail("FieldExitPlacementOriginalHeadRequired", bound.actor_id);
  auto actual = take(original->read_field_exit_placement(
      {bound.actual_source, 1}, bound.actor_id));
  if (actual.actor_id != bound.actor_id ||
      actual.immutable_source_head.size() != 1 ||
      actual.immutable_source_head.front().schema != 1 ||
      !same_source(actual.immutable_source_head.front().source,
                   bound.actual_source))
    fail("FieldExitPlacementOriginalIdentityMismatch", bound.actor_id);
  const auto &anchor_source = std::visit(
      [](const auto &anchor) -> const SourceRef & {
        using T = std::decay_t<decltype(anchor)>;
        if constexpr (std::is_same_v<T, TerrainContinuousAnchor>)
          return anchor.actual_prefix_close_source;
        else
          return anchor.actual_source;
      },
      actual.actual_anchor);
  if (!same_source(anchor_source, bound.actual_source))
    fail("FieldExitPlacementAnchorSourceMismatch", bound.actor_id);
  take(compare_world_moments(actual.occurred_at, actual.occurred_at, budget));
  return actual;
}
void require_field_exit_mutation(const BoundSpatialLocationWrite &bound,
                                 const ActorLocationExecutionDraft &draft) {
  if (is_field_battle_exit_placement_source(bound.actual_source) &&
      (draft.kind != ActorLocationExecutionDraft::Kind::mutation ||
       !draft.before || bound.prefix_witness))
    fail("FieldExitPlacementActualMutationRequired", bound.actor_id);
}
// 独立死俘交接只借真实业务行；此读不替代当前 Common capture 的身份核对。
FieldActorMemberLossPlacementRead
read_field_loss(const CommonStore &common, const MovementExactState &movement,
                const SourceRef &source, Integer schema, const StableId &actor,
                const FieldBattleExitPlacementRead *original,
                ExactArithmeticBudget budget) {
  if (!original || !original->bound_to(common, movement) || schema != 1 ||
      !is_field_member_loss_placement_source(source))
    fail("FieldMemberLossOriginalReadRequired", actor);
  const auto *fact = common.state().facts.find(source.id);
  if (!fact || fact->domain != source.domain || fact->kind != source.kind)
    fail("FieldMemberLossOriginalHeadRequired", actor);


  auto input_memory = CheckpointMemoryCharge(0);
  const MomentOwnerSource input{{checkpoint_fresh_string_copy(source.domain),
                                 checkpoint_fresh_string_copy(source.kind),
                                 checkpoint_fresh_string_copy(source.id)},
                                1};
  auto actual = take(original->read_field_member_loss_placement(input, actor));
  const auto &placement = actual.actual_placement;
  if (!actual.original_loss || actual.original_loss->id != source.id ||
      actual.original_loss->actor_id != actor || placement.actor_id != actor ||
      placement.immutable_source_head.size() != 1 ||
      placement.immutable_source_head.front().schema != 1 ||
      !same_source(placement.immutable_source_head.front().source, source) ||
      take(compare_world_moments(placement.occurred_at,
                                 actual.original_loss->occurred_at, budget)))
    fail("FieldMemberLossOriginalIdentityMismatch", actor);
  const bool captured = std::holds_alternative<GroupMemberCaptivityApplication>(
      actual.original_loss->actual_body);
  if (bool(actual.carried_army_id) != captured ||
      (actual.carried_army_id && actual.carried_army_id->empty()))
    fail("FieldMemberLossOriginalCarrierMismatch", actor);
  return actual;
}
// 比较原投影时保留该次临时副本的费用。俘虏的 Common 字段只核真实 Army ID，
// physical anchor 另核，绝不构造 InArmy 类型的静置 anchor。
bool field_loss_location_matches(
    const FieldActorMemberLossPlacementRead &actual, const Location &location,
    ExactArithmeticBudget budget) {
  if (actual.carried_army_id) {
    const auto *army = std::get_if<InArmyLocation>(&location);
    return army && army->army_id == *actual.carried_army_id;
  }

  std::visit(
      [&](const auto &anchor) {
        using T = std::decay_t<decltype(anchor)>;
        if constexpr (std::is_same_v<T, OriginalMovementAnchor>) {
          std::visit(
              [&](const auto &location) {
                using L = std::decay_t<decltype(location)>;
                if constexpr (std::is_same_v<L, SettlementLocation>) {


                } else if constexpr (std::is_same_v<L, WildernessLocation>) {


                } else if constexpr (std::is_same_v<L, OnRouteLocation>) {


                } else {
                  fail("FieldMemberLossPhysicalAnchorRequired");
                }
              },
              anchor.location);
        } else if constexpr (std::is_same_v<T, RoadFractionalAnchor>) {


        } else {


        }
      },
      actual.actual_placement.actual_anchor);
  auto projection_memory = CheckpointMemoryCharge(0);
  const auto projected = take(
      project_movement_location(actual.actual_placement.actual_anchor, budget));
  return same_location(location, projected);
}
// 共享完整 capture 和逐笔读取均消费同一个原死俘 source/位置比较算法。
bool field_loss_draft_matches(const CommonStore &common,
                              const MovementExactState &movement,
                              const BoundSpatialLocationWrite &bound,
                              const ActorLocationExecutionDraft &draft,
                              const FieldBattleExitPlacementRead *original,
                              ExactArithmeticBudget budget) {
  if (draft.kind != ActorLocationExecutionDraft::Kind::mutation ||
      !draft.before || bound.prefix_witness)
    fail("FieldMemberLossActualMutationRequired", bound.actor_id);
  const auto actual =
      read_field_loss(common, movement, bound.actual_source,
                      bound.source_schema, bound.actor_id, original, budget);
  return field_loss_location_matches(actual, draft.after, budget) &&
         same_anchor(actual.actual_placement.actual_anchor, bound.actual_anchor,
                     budget);
}
// 仅作候选分类；普通cancel没有完整ready provider仍会拒绝，不能靠kind获得落点。
bool field_stable_candidate(const SourceRef &source) {
  return source.domain == "travel" && source.kind == "group_movement_control";
}
// 双原头来自同World完整ready/cancel读口；期望Army只借原plan，不从今日Actor反推。
FieldActorArmyExitStablePlacementRead read_field_stable(
    const CommonStore &common, const MovementExactState &movement,
    const SourceRef &source, Integer schema, const StableId &actor,
    const FieldBattleExitPlacementRead *original, ExactArithmeticBudget b) {
  if (!original || !original->bound_to(common, movement) || schema != 1 ||
      !field_stable_candidate(source))
    fail("FieldArmyExitStablePlacementOriginalReadRequired", actor);
  auto actual = take(original->read_field_army_exit_stable_placement({source, schema}, actor));
  const auto &point = actual.actual;
  if (!actual.army_id || actual.army_id->empty() || point.actor_id != actor ||
      point.immutable_source_head.size() != 2 ||
      point.immutable_source_head[0].schema != 1 || point.immutable_source_head[1].schema != 1 ||
      !same_source(point.immutable_source_head[0].source, source) ||
      point.immutable_source_head[1].source.domain != "military" ||
      point.immutable_source_head[1].source.kind != "field_army_exit_ready")
    fail("FieldArmyExitStablePlacementOriginalIdentityMismatch", actor);
  for (const auto &ref : point.immutable_source_head) {
    const auto *head = common.state().facts.find(ref.source.id);
    if (!head || head->id != ref.source.id || head->domain != ref.source.domain || head->kind != ref.source.kind)
      fail("FieldArmyExitStablePlacementOriginalHeadRequired", actor);
  }
  take(compare_world_moments(point.occurred_at, point.occurred_at, b));
  return actual;
}
// 普通兵/早退出成员同样只允许一次真mutation重绑原Army，不把真实地点写成卸装位置。
bool field_stable_draft_matches(const CommonStore &common, const MovementExactState &movement,
    const BoundSpatialLocationWrite &bound, const ActorLocationExecutionDraft &draft,
    const FieldBattleExitPlacementRead *original, ExactArithmeticBudget b) {
  if (draft.kind != ActorLocationExecutionDraft::Kind::mutation || !draft.before || bound.prefix_witness)
    fail("FieldArmyExitStablePlacementActualMutationRequired", bound.actor_id);
  const auto actual = read_field_stable(common, movement, bound.actual_source,
      bound.source_schema, bound.actor_id, original, b);
  const auto *in_army = std::get_if<InArmyLocation>(&draft.after);
  return in_army && in_army->army_id == *actual.army_id &&
      same_anchor(actual.actual.actual_anchor, bound.actual_anchor, b);
}
const TravelExertionBasis &
army_opening_effort(const MovementExactState &state,
                    const BoundSpatialLocationWrite &bound) {
  const TravelExertionBasis *actual = nullptr;
  for (const auto &row : state.exertion_bases.rows()) {
    const auto &effort = row.second;
    const auto *unit = state.units.find(effort.unit_ref);
    const auto *execution =
        std::get_if<GroupMovementExecution>(&effort.execution);
    if (unit && unit->ordinal == 0 && execution &&
        std::holds_alternative<ArmyCarrierRef>(execution->carrier) &&
        same_source(effort.original_rule_source, bound.actual_source) &&
        std::find(effort.actual_participants.begin(),
                  effort.actual_participants.end(),
                  bound.actor_id) != effort.actual_participants.end()) {
      if (actual)
        fail("AmbiguousArmyOpeningSpatialSource", bound.actor_id);
      actual = &effort;
    }
  }
  if (!actual)
    fail("ArmyOpeningSpatialSourceRequired", bound.actor_id);
  return *actual;
}
// local-move 的 tap 必须同时核实际 before/after，不能只凭当前终点背书。
bool local_move_draft_matches(const CommonStore& common,const MovementExactState& state,
    const BoundSpatialLocationWrite& bound,const ActorLocationExecutionDraft& draft,
    ExactArithmeticBudget b) {
  if(bound.actual_source.domain!="travel" || bound.actual_source.kind!="local_move") return false;
  const auto* row=take(read_local_move_source(common,state,bound.actual_source,bound.source_schema,b));
  return row->actor_id==bound.actor_id && draft.before &&
      same_location(*draft.before,Location{row->from}) &&
      same_location(draft.after,Location{row->to}) &&
      same_anchor(bound.actual_anchor,MovementAnchor{OriginalMovementAnchor{row->to,bound.actual_source}},b);
}
MovementAnchor source_anchor(
    const MovementExactState &state, const BoundSpatialLocationWrite &bound,
    ExactArithmeticBudget b, const CommonStore *common = nullptr,
    const SpatialCarrierState *carriers = nullptr,
    const TransportRegistrationState *registrations = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr) {
  if (is_field_battle_exit_placement_source(bound.actual_source)) {
    if (!common)
      fail("FieldExitPlacementCommonRequired", bound.actor_id);
    return read_field_exit(*common, state, bound, field_exits, b).actual_anchor;
  }
  if (is_named_military_custody_placement_source(bound.actual_source)) {
    if (bound.source_schema != 1 || !common || !custody_releases ||
        !custody_releases->bound_to(*common, state))
      fail("NamedMilitaryCustodyPlacementOriginalReadRequired", bound.actor_id);
    return take(custody_releases->read_named_custody_placement(
                    bound.actual_source, bound.actor_id))
        ->released_anchor;
  }
  if (is_military_action_placement_source_kind(bound.actual_source)) {
    if (!common || !military_actions ||
        !military_actions->bound_to(*common, state))
      fail("MilitaryActionPlacementOriginalReadRequired", bound.actor_id);
    const auto actual = take(military_actions->read_military_action_placement(
        {bound.actual_source, bound.source_schema}, bound.actor_id));
    if (!actual)
      fail("MilitaryActionPlacementActorMismatch", bound.actor_id);
    return actual->actual_anchor;
  }
  if (bound.actual_source.domain == "travel" &&
      bound.actual_source.kind == "travel_dependent_binding_change") {
    if (!births || !common || !births->bound_to(*common))
      fail("BirthPlacementCaptureOriginalReadRequired", bound.actor_id);
    const auto actual = take(births->read_birth_placement_result(
        {bound.actual_source, bound.source_schema}));
    if (!actual.actual_placement || actual.child_actor_id != bound.actor_id)
      fail("BirthPlacementCaptureActorMismatch", bound.actor_id);
    return actual.actual_placement->resolved_anchor;
  }
  if (bound.source_schema != 1 || bound.actual_source.domain != "travel")
    fail("UnsupportedSpatialWriteSource", bound.actual_source.id);
  if (bound.actual_source.kind == "local_move") {
    if(!common) fail("LocalMoveOriginalCommonRequired",bound.actual_source.id);
    const auto* row=take(read_local_move_source(*common,state,bound.actual_source,bound.source_schema,b));
    if(row->actor_id!=bound.actor_id) fail("LocalMoveOriginalActorMismatch",bound.actor_id);
    return OriginalMovementAnchor{row->to,bound.actual_source};
  }
  if (bound.actual_source.kind == "transport_registration" ||
      bound.actual_source.kind == "transport_release") {
    if (!common || !carriers || !registrations)
      fail("TransportSpatialSourceReadRequired", bound.actual_source.id);
    const auto *head = common->state().facts.find(bound.actual_source.id);
    if (!head || head->domain != bound.actual_source.domain ||
        head->kind != bound.actual_source.kind)
      fail("TransportSpatialSourceHeadRequired", bound.actual_source.id);
    if (bound.actual_source.kind == "transport_registration") {
      const auto *source =
          registrations->registrations.find(bound.actual_source.id);
      const auto *unit =
          source ? carriers->transport_units.find(source->transport_unit_id)
                 : nullptr;
      if (!source || !unit ||
          !same_source(unit->registration_source_ref, bound.actual_source) ||
          std::find(source->accepted_scope.named_participants.begin(),
                    source->accepted_scope.named_participants.end(),
                    bound.actor_id) ==
              source->accepted_scope.named_participants.end())
        fail("TransportRegistrationActorSourceMismatch", bound.actor_id);
      return source->departure_anchor;
    }
    const auto *source = registrations->releases.find(bound.actual_source.id);
    const auto *unit =
        source ? carriers->transport_units.find(source->transport_unit_id)
               : nullptr;
    if (!source || !unit ||
        std::find(source->released_composition.named_participants.begin(),
                  source->released_composition.named_participants.end(),
                  bound.actor_id) ==
            source->released_composition.named_participants.end() ||
        !source_valid(source->accepted_handover_source))
      fail("TransportReleaseActorSourceMismatch", bound.actor_id);
    return source->actual_anchor;
  }
  if (bound.actual_source.kind == "travel_exertion_basis") {
    const auto *effort = state.exertion_bases.find(bound.actual_source.id);
    const auto *unit = effort ? state.units.find(effort->unit_ref) : nullptr;
    if (!effort || !unit || unit->ordinal != 0 ||
        unit->effort_basis_refs.empty() ||
        !same_source(unit->effort_basis_refs.front(), bound.actual_source) ||
        !effort->original_action ||
        effort->original_action->actor_id != bound.actor_id ||
        execution_action(unit->execution) != effort->original_action->id)
      fail("SpatialOpeningSourceRequired", bound.actor_id);
    if (common) {
      const auto *head = common->state().facts.find(effort->id);
      if (!head || head->domain != "travel" ||
          head->kind != "travel_exertion_basis")
        fail("SpatialOpeningSourceHeadRequired", effort->id);
    }
    MovementPrefixState opening;
    opening.basis = effort->actual_basis;
    opening.cursor = effort->opened_cursor;
    opening.lifecycle = MovementActive{};
    return take(read_movement_position(opening, b));
  }
  if (bound.actual_source.kind == "group_movement_opening") {
    if (common) {
      const auto *head = common->state().facts.find(bound.actual_source.id);
      if (!head || head->kind != bound.actual_source.kind ||
          head->domain != bound.actual_source.domain)
        fail("ArmyOpeningSpatialSourceHeadRequired", bound.actual_source.id);
    }
    const auto &effort = army_opening_effort(state, bound);
    MovementPrefixState initial;
    initial.basis = effort.actual_basis;
    initial.cursor = effort.opened_cursor;
    initial.lifecycle = MovementActive{};
    return take(read_movement_position(initial, b));
  }
  if (bound.actual_source.kind == "movement_prefix") {
    if (!bound.prefix_witness ||
        bound.prefix_witness->id != bound.actual_source.id)
      fail("SpatialPrefixWitnessRequired", bound.actor_id);
    const auto &witness = *bound.prefix_witness;
    const auto *current = state.prefixes.find(witness.id);
    const auto *unit = state.units.find(witness.unit_ref);
    const TravelExertionBasis *effort = nullptr;
    if (unit)
      for (const auto &ref : unit->effort_basis_refs) {
        const auto *candidate = state.exertion_bases.find(ref.id);
        if (candidate &&
            basis_id(candidate->actual_basis) == basis_id(witness.basis))
          effort = candidate;
      }
    if (!current || !unit || !effort ||
        std::find(effort->actual_participants.begin(),
                  effort->actual_participants.end(),
                  bound.actor_id) == effort->actual_participants.end() ||
        execution_key(current->execution) != execution_key(witness.execution) ||
        execution_key(unit->execution) != execution_key(witness.execution) ||
        basis_id(effort->actual_basis) != basis_id(witness.basis))
      fail("SpatialPrefixWitnessMismatch", bound.actor_id);
    const auto *mapping =
        std::get_if<RunningMovementMap>(&witness.activity_world_map);
    if (!mapping ||
        compare(add(mapping->cursor_at_from,
                    mul(mapping->rate,
                        take(elapsed_world_duration(mapping->world_from,
                                                    witness.last_settled_at, b))
                            .value,
                        b),
                    b),
                witness.cursor, b) != 0)
      fail("SpatialPrefixCursorMismatch", bound.actor_id);
    return take(read_movement_position(witness, b));
  }
  if (bound.actual_source.kind == "movement_arrival") {
    auto source =
        take(read_verified_movement_arrival(state, bound.actual_source, 1, b));
    const auto *unit = state.units.find(source.unit_ref);
    const auto *effort =
        unit && unit->effort_basis_refs.size() > 0
            ? state.exertion_bases.find(unit->effort_basis_refs.front().id)
            : nullptr;
    if (!effort ||
        std::find(effort->actual_participants.begin(),
                  effort->actual_participants.end(),
                  bound.actor_id) == effort->actual_participants.end())
      fail("SpatialArrivalActorMismatch", bound.actor_id);
    return source.actual_anchor;
  }
  if (bound.actual_source.kind == "movement_contact") {
    const auto *source = state.contacts.find(bound.actual_source.id);
    if (!source)
      fail("SpatialContactSourceRequired", bound.actor_id);
    return take(
        read_exact_contact_actor_anchor(state, *source, bound.actor_id, b));
  }
  fail("UnsupportedSpatialWriteSource", bound.actual_source.id);
}
Location source_location(
    const MovementExactState &state, const BoundSpatialLocationWrite &bound,
    ExactArithmeticBudget b, const TransportRegistrationState *registrations,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr) {
  if (is_military_action_placement_source_kind(bound.actual_source)) {
    if (!military_actions)
      fail("MilitaryActionPlacementOriginalReadRequired", bound.actor_id);
    const auto actual = take(military_actions->read_military_action_placement(
        {bound.actual_source, bound.source_schema}, bound.actor_id));
    if (!actual)
      fail("MilitaryActionPlacementActorMismatch", bound.actor_id);
    return actual->original_location_result;
  }
  if (bound.actual_source.domain == "travel" &&
      bound.actual_source.kind == "travel_dependent_binding_change") {
    if (!births)
      fail("BirthPlacementCaptureOriginalReadRequired", bound.actor_id);
    const auto actual = take(births->read_birth_placement_result(
        {bound.actual_source, bound.source_schema}));
    if (!actual.actual_placement || actual.child_actor_id != bound.actor_id)
      fail("BirthPlacementCaptureActorMismatch", bound.actor_id);
    return actual.actual_placement->location;
  }
  if (bound.actual_source.domain == "travel" &&
      bound.actual_source.kind == "group_movement_opening") {
    const auto &effort = army_opening_effort(state, bound);
    return InArmyLocation{
        std::get<ArmyCarrierRef>(
            std::get<GroupMovementExecution>(effort.execution).carrier)
            .army_id};
  }
  if (bound.actual_source.domain == "travel" &&
      bound.actual_source.kind == "transport_registration") {
    const auto *source =
        registrations
            ? registrations->registrations.find(bound.actual_source.id)
            : nullptr;
    if (!source)
      fail("TransportRegistrationSourceRequired", bound.actual_source.id);
    CarriedLocation carried;
    carried.carrier_kind = decltype(carried.carrier_kind)::transport_unit;
    carried.carrier_id = source->transport_unit_id;
    return carried;
  }
  return take(project_movement_location(bound.actual_anchor, b));
}
Status
validate_capture(const CommonStore &common, const MovementExactState &state,
                 const SpatialCarrierState *carriers,
                 const TransportRegistrationState *registrations,
                 const std::vector<BoundSpatialLocationWrite> &bindings,
                 ExactArithmeticBudget b, const BirthPlacementRead *births,
                 const MilitaryActionPlacementRead *military_actions,
                 const NamedMilitaryCustodyPlacementRead *custody_releases,
                 const FieldBattleExitPlacementRead *field_exits) {
  return status([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!identity || bindings.size() != drafts.size()) {
      // 只在原覆盖拒绝前打印候选原 tap；不补绑定、不消费来源，原错误仍权威。
      // 不复制 checkpoint 字段；最多前 32 条，原总数明确，诊断失败不改判定。
      std::fprintf(stderr, "SPATIAL_CAPTURE_DIAGNOSTIC active=%d bindings=%zu drafts=%zu limit=32\n",
                   identity ? 1 : 0, bindings.size(), drafts.size());
      for (std::size_t n = 0; n < drafts.size() && n < 32; ++n) {
        const auto &d = drafts[n];
        std::size_t matched = 0;
        for (const auto &bound : bindings)
          if (bound.capture_identity == identity &&
              bound.write_identity == d.write_identity &&
              bound.capture_sequence == d.sequence && bound.actor_id == d.actor_id)
            ++matched;
        const auto *before = d.before ? std::get_if<SettlementLocation>(&*d.before) : nullptr;
        const auto *after = std::get_if<SettlementLocation>(&d.after);
        std::fprintf(stderr,
                     "SPATIAL_CAPTURE_TAP index=%zu sequence=%lld actor=%s matched=%zu before_kind=%lld after_kind=%zu before_place=%s after_place=%s\n",
                     n, static_cast<long long>(d.sequence), d.actor_id.c_str(), matched,
                     d.before ? static_cast<long long>(d.before->index()) : -1LL,
                     d.after.index(), before ? before->place_id.c_str() : "<not-settlement>",
                     after ? after->place_id.c_str() : "<not-settlement>");
      }
      for (std::size_t n = 0; n < bindings.size() && n < 32; ++n) {
        const auto &bound = bindings[n];
        std::fprintf(stderr,
                     "SPATIAL_CAPTURE_BOUND index=%zu sequence=%lld actor=%s source=%s/%s/%s same_capture=%d\n",
                     n, static_cast<long long>(bound.capture_sequence), bound.actor_id.c_str(),
                     bound.actual_source.domain.c_str(), bound.actual_source.kind.c_str(),
                     bound.actual_source.id.c_str(), bound.capture_identity == identity ? 1 : 0);
      }
      fail("SpatialLocationCaptureCoverageMismatch");
    }
    for (std::size_t i = 0; i < drafts.size(); ++i) {
      const auto &draft = drafts[i];
      const auto &bound = bindings[i];
      require_field_exit_mutation(bound, draft);
      if (bound.actual_source.domain == "travel" &&
          bound.actual_source.kind == "travel_dependent_binding_change" &&
          (draft.kind != ActorLocationExecutionDraft::Kind::insertion ||
           draft.before))
        fail("BirthPlacementActualInsertionMismatch", draft.actor_id);
      if (!bound.write_identity ||
          bound.write_identity != draft.write_identity ||
          bound.capture_identity != identity ||
          bound.capture_sequence != Integer(i + 1) ||
          draft.sequence != bound.capture_sequence ||
          draft.actor_id != bound.actor_id ||
          !(bound.actual_source.domain=="travel" && bound.actual_source.kind=="local_move"
              ? local_move_draft_matches(common,state,bound,draft,b)
              : is_field_member_loss_placement_source(bound.actual_source)
                ? field_loss_draft_matches(common, state, bound, draft,
                                           field_exits, b)
                : field_stable_candidate(bound.actual_source)
                    ? field_stable_draft_matches(common, state, bound, draft, field_exits, b)
                : (same_location(draft.after,
                                 source_location(state, bound, b, registrations,
                                                 births, military_actions,
                                                 custody_releases)) &&
                   same_anchor(source_anchor(state, bound, b, &common, carriers,
                                             registrations, births,
                                             military_actions, custody_releases,
                                             field_exits),
                               bound.actual_anchor, b))))
        fail("SpatialLocationCaptureBindingMismatch", draft.actor_id);
    }
  });
}

} // namespace
Outcome<ActualActorSpatialWriteRead> read_actual_actor_spatial_write(
    const CommonStore &common, const MovementExactState &state,
    const BoundSpatialLocationWrite &bound, const SpatialCarrierState *carriers,
    const TransportRegistrationState *registrations, ExactArithmeticBudget b,
    const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<ActualActorSpatialWriteRead>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!identity || bound.capture_identity != identity ||
        bound.capture_sequence < 1 ||
        std::size_t(bound.capture_sequence) > drafts.size())
      fail("SpatialLocationCaptureRequired", bound.actor_id);
    const auto &draft = drafts[std::size_t(bound.capture_sequence - 1)];
    require_field_exit_mutation(bound, draft);
    if (!bound.write_identity || bound.write_identity != draft.write_identity ||
        draft.sequence != bound.capture_sequence ||
        draft.actor_id != bound.actor_id ||
        !(bound.actual_source.domain=="travel" && bound.actual_source.kind=="local_move"
              ? local_move_draft_matches(common,state,bound,draft,b)
              : is_field_member_loss_placement_source(bound.actual_source)
              ? field_loss_draft_matches(common, state, bound, draft,
                                         field_exits, b)
              : field_stable_candidate(bound.actual_source)
                  ? field_stable_draft_matches(common, state, bound, draft, field_exits, b)
              : (same_location(draft.after,
                               source_location(state, bound, b, registrations,
                                               births, military_actions,
                                               custody_releases)) &&
                 same_anchor(source_anchor(state, bound, b, &common, carriers,
                                           registrations, births,
                                           military_actions, custody_releases,
                                           field_exits),
                             bound.actual_anchor, b))))
      fail("SpatialLocationCaptureBindingMismatch", bound.actor_id);
    // 稳定采用必须返回cancel+ready双原头，不能走下面旧单头归约丢掉真正落点来源。
    if (field_stable_candidate(bound.actual_source))
      return read_field_stable(common, state, bound.actual_source,
          bound.source_schema, bound.actor_id, field_exits, b).actual;
    WorldMoment occurred_at;
    ActualSpatialSourceBuildView head{{bound.actual_source.domain,
                                       bound.actual_source.kind,
                                       bound.actual_source.id},
                                      bound.source_schema};
    if (is_field_member_loss_placement_source(bound.actual_source)) {
      occurred_at =
          read_field_loss(common, state, bound.actual_source,
                          bound.source_schema, bound.actor_id, field_exits, b)
              .actual_placement.occurred_at;
    } else if (is_field_battle_exit_placement_source(bound.actual_source)) {
      occurred_at =
          read_field_exit(common, state, bound, field_exits, b).occurred_at;
    } else if (is_named_military_custody_placement_source(
                   bound.actual_source)) {
      if (bound.source_schema != 1 || !custody_releases ||
          !custody_releases->bound_to(common, state))
        fail("NamedMilitaryCustodyPlacementOriginalReadRequired",
             bound.actor_id);
      occurred_at = take(custody_releases->read_named_custody_placement(
                             bound.actual_source, bound.actor_id))
                        ->released_at;
    } else if (is_military_action_placement_source_kind(bound.actual_source)) {
      if (!military_actions || !military_actions->bound_to(common, state))
        fail("MilitaryActionPlacementOriginalReadRequired", bound.actor_id);
      const auto actual = take(military_actions->read_military_action_placement(
          {bound.actual_source, bound.source_schema}, bound.actor_id));
      if (!actual)
        fail("MilitaryActionPlacementActorMismatch", bound.actor_id);
      occurred_at = actual->occurred_at;
    } else if (bound.actual_source.kind == "travel_dependent_binding_change") {
      const auto actual = take(births->read_birth_placement_result(
          {bound.actual_source, bound.source_schema}));
      occurred_at = actual.occurred_at;
    } else if (bound.actual_source.kind == "movement_prefix") {
      const auto &witness = *bound.prefix_witness;
      const auto *unit = state.units.find(witness.unit_ref);
      const TravelExertionBasis *effort = nullptr;
      const SourceRef *effort_ref = nullptr;
      for (const auto &ref : unit->effort_basis_refs) {
        const auto *value = state.exertion_bases.find(ref.id);
        if (value && basis_id(value->actual_basis) == basis_id(witness.basis) &&
            std::find(value->actual_participants.begin(),
                      value->actual_participants.end(),
                      bound.actor_id) != value->actual_participants.end()) {
          if (effort)
            fail("AmbiguousSpatialWriteEffort", bound.actor_id);
          effort = value;
          effort_ref = &ref;
        }
      }
      if (!effort)
        fail("SpatialWriteEffortRequired", bound.actor_id);
      bool covered = false;
      for (const auto &work : unit->activity_spans)
        if (work.basis_ref == basis_id(witness.basis) &&
            take(compare_world_moments(work.span.from, witness.last_settled_at,
                                       b)) < 0 &&
            take(compare_world_moments(witness.last_settled_at, work.span.to,
                                       b)) <= 0) {
          if (take(compare_world_moments(work.span.from, work.span.to, b)) >=
                  0 ||
              compare(work.cursor_to,
                      add(work.cursor_from,
                          mul(work.rate,
                              take(elapsed_world_duration(work.span.from,
                                                          work.span.to, b))
                                  .value,
                              b),
                          b),
                      b))
            fail("SpatialWriteActivityMismatch", unit->id);
          const auto actual =
              add(work.cursor_from,
                  mul(work.rate,
                      take(elapsed_world_duration(work.span.from,
                                                  witness.last_settled_at, b))
                          .value,
                      b),
                  b);
          if (compare(actual, witness.cursor, b))
            fail("SpatialWriteActivityCursorMismatch", unit->id);
          covered = true;
        }
      if (!covered)
        fail("SpatialWriteActualActivityRequired", unit->id);
      take(read_movement_source_moment(state, *effort_ref, 1, b));
      occurred_at = witness.last_settled_at;
      if (effort->original_action) {
        const auto original = take(read_personal_movement_immutable_head_view(
            common, state, witness, bound.actor_id, b));
        head = {original.source_ref, original.source_schema};
      } else
        head = {{effort_ref->domain, effort_ref->kind, effort_ref->id}, 1};
    } else if (bound.actual_source.kind == "group_movement_opening") {
      occurred_at = army_opening_effort(state, bound).opened_at;
      head.schema = 1;
    } else if (bound.actual_source.kind == "transport_registration") {
      occurred_at = registrations->registrations.find(bound.actual_source.id)
                        ->occurred_at;
      head.schema = 1;
    } else if (bound.actual_source.kind == "transport_release") {
      occurred_at =
          registrations->releases.find(bound.actual_source.id)->occurred_at;
      head.schema = 1;
    } else
      occurred_at = take(read_movement_source_moment(state, bound.actual_source,
                                                     bound.source_schema, b));
    // Every original branch selects one real immutable head. Borrow that input
    // until the output's own final-count payload plan precedes its first copy.
    CheckpointVector<ActualSpatialSourceBuildView> heads;
    heads.reserve(1);
    heads.push_back(head);
    return ActualActorSpatialWriteRead{ActualActorSpatialWriteReadBuildView{
        bound.actor_id, occurred_at, bound.actual_anchor, heads}};
  });
}
Outcome<BoundSpatialLocationWrite> bind_actual_spatial_location_write(
    const CommonStore &common, const MovementExactState &state,
    const StableId &actor, const SourceRef &source, Integer schema,
    const MovementAnchor &anchor, Integer sequence, ExactArithmeticBudget b) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!identity || drafts.empty())
      fail("SpatialLocationCaptureRequired", actor);
    if (sequence == 0)
      sequence = Integer(drafts.size());
    if (sequence < 1 || std::size_t(sequence) > drafts.size())
      fail("SpatialCaptureSequenceMismatch", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    if (draft.actor_id != actor || draft.sequence != sequence)
      fail("SpatialActualWriteMismatch", actor);
    const bool needs_prefix =
        source.domain == "travel" && source.kind == "movement_prefix";
    const auto *witness =
        needs_prefix ? state.prefixes.find(source.id) : nullptr;
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.domain, source.kind, source.id},
        schema,
        anchor,
        identity,
        draft.sequence,
        witness,
        draft.write_identity}};
    if (!out.write_identity)
      fail("SpatialWriteIdentityRequired", actor);
    if(source.domain=="travel" && source.kind=="local_move" &&
       !local_move_draft_matches(common,state,out,draft,b))
      fail("LocalMoveActualWriteMismatch",actor);
    if (!same_location(draft.after, source_location(state, out, b, nullptr)))
      fail("SpatialActualWriteMismatch", actor);
    if (needs_prefix && !witness)
      fail("SpatialPrefixWitnessRequired", actor);
    if (!same_anchor(source_anchor(state, out, b, &common), out.actual_anchor,
                     b))
      fail("SpatialActualSourceAnchorMismatch", actor);
    return out;
  });
}
Outcome<BoundSpatialLocationWrite> bind_actual_transport_location_write(
    const CommonStore &common, const MovementExactState &state,
    const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations, const StableId &actor,
    const SourceRef &source, const MovementAnchor &anchor, Integer sequence,
    ExactArithmeticBudget b) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!identity || drafts.empty())
      fail("SpatialLocationCaptureRequired", actor);
    if (sequence == 0)
      sequence = Integer(drafts.size());
    if (sequence < 1 || std::size_t(sequence) > drafts.size())
      fail("SpatialCaptureSequenceMismatch", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.domain, source.kind, source.id},
        1,
        anchor,
        identity,
        sequence,
        nullptr,
        draft.write_identity}};
    if (!out.write_identity || draft.actor_id != actor ||
        draft.sequence != sequence ||
        !same_location(draft.after,
                       source_location(state, out, b, &registrations)) ||
        !same_anchor(
            source_anchor(state, out, b, &common, &carriers, &registrations),
            anchor, b))
      fail("SpatialActualWriteMismatch", actor);
    return out;
  });
}
Status validate_spatial_location_capture(
    const CommonStore &common, const MovementExactState &state,
    const std::vector<BoundSpatialLocationWrite> &bindings,
    ExactArithmeticBudget b, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return validate_capture(common, state, nullptr, nullptr, bindings, b, births,
                          military_actions, custody_releases, field_exits);
}
Status validate_spatial_location_capture(
    const CommonStore &common, const MovementExactState &state,
    const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const std::vector<BoundSpatialLocationWrite> &bindings,
    ExactArithmeticBudget b, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return validate_capture(common, state, &carriers, &registrations, bindings, b,
                          births, military_actions, custody_releases,
                          field_exits);
}
Outcome<BoundSpatialLocationWrite> bind_actual_birth_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const BirthPlacementRead &births, const StableId &actor,
    const MomentOwnerSource &ref, const MovementAnchor &anchor,
    Integer sequence, ExactArithmeticBudget budget) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!births.bound_to(common) || !identity || sequence < 1 ||
        std::size_t(sequence) > drafts.size())
      fail("BirthPlacementActualCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {ref.source_ref.domain, ref.source_ref.kind, ref.source_ref.id},
        ref.source_schema,
        anchor,
        identity,
        sequence,
        nullptr,
        draft.write_identity}};
    if (draft.kind != ActorLocationExecutionDraft::Kind::insertion ||
        draft.before || draft.actor_id != actor || draft.sequence != sequence ||
        !out.write_identity ||
        !same_location(draft.after, source_location(movement, out, budget,
                                                    nullptr, &births)) ||
        !same_anchor(source_anchor(movement, out, budget, &common, nullptr,
                                   nullptr, &births),
                     anchor, budget))
      fail("BirthPlacementActualInsertionMismatch", actor);
    return out;
  });
}

Outcome<BoundSpatialLocationWrite> bind_actual_military_action_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const MilitaryActionPlacementRead &original, const StableId &actor,
    const MomentOwnerSource &source, const MovementAnchor &anchor,
    Integer sequence, ExactArithmeticBudget budget) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!original.bound_to(common, movement) || !identity || sequence < 1 ||
        std::size_t(sequence) > drafts.size())
      fail("MilitaryActionPlacementCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.source_ref.domain, source.source_ref.kind,
         source.source_ref.id},
        source.source_schema,
        anchor,
        identity,
        sequence,
        nullptr,
        draft.write_identity}};
    if (draft.kind != ActorLocationExecutionDraft::Kind::mutation ||
        !draft.before || draft.actor_id != actor ||
        draft.sequence != sequence || !out.write_identity ||
        !same_location(draft.after,
                       source_location(movement, out, budget, nullptr, nullptr,
                                       &original)) ||
        !same_anchor(source_anchor(movement, out, budget, &common, nullptr,
                                   nullptr, nullptr, &original),
                     anchor, budget))
      fail("MilitaryActionPlacementActualWriteMismatch", actor);
    return out;
  });
}

// 同一个候选的实际 Common mutation 与唯一释放正文逐项相认；旧捕获历史不改写。
Outcome<BoundSpatialLocationWrite> bind_actual_named_custody_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const NamedMilitaryCustodyPlacementRead &original, const StableId &actor,
    const SourceRef &source, const MovementAnchor &anchor, Integer sequence,
    ExactArithmeticBudget budget) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!original.bound_to(common, movement) || !identity || sequence < 1 ||
        std::size_t(sequence) > drafts.size())
      fail("NamedMilitaryCustodyPlacementCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.domain, source.kind, source.id},
        1,
        anchor,
        identity,
        sequence,
        nullptr,
        draft.write_identity}};
    if (draft.kind != ActorLocationExecutionDraft::Kind::mutation ||
        !draft.before || draft.actor_id != actor ||
        draft.sequence != sequence || !out.write_identity ||
        !same_location(draft.after,
                       source_location(movement, out, budget, nullptr, nullptr,
                                       nullptr, &original)) ||
        !same_anchor(source_anchor(movement, out, budget, &common, nullptr,
                                   nullptr, nullptr, nullptr, &original),
                     anchor, budget))
      fail("NamedMilitaryCustodyPlacementActualWriteMismatch", actor);
    return out;
  });
}

// 原位置 writer 已执行后才绑定真实 tap；原 group 的成员释放另由同 capture
// 采用。
Outcome<BoundSpatialLocationWrite> bind_actual_field_exit_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const FieldBattleExitPlacementRead &original, const StableId &actor,
    const MomentOwnerSource &source, const MovementAnchor &anchor,
    Integer sequence, ExactArithmeticBudget budget) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!original.bound_to(common, movement) || !identity || sequence < 1 ||
        std::size_t(sequence) > drafts.size())
      fail("FieldExitPlacementCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.source_ref.domain, source.source_ref.kind,
         source.source_ref.id},
        source.source_schema,
        anchor,
        identity,
        sequence,
        nullptr,
        draft.write_identity}};
    require_field_exit_mutation(out, draft);
    const auto actual =
        read_field_exit(common, movement, out, &original, budget);
    if (draft.actor_id != actor || draft.sequence != sequence ||
        !out.write_identity ||
        !same_location(draft.after, take(project_movement_location(
                                        actual.actual_anchor, budget))) ||
        !same_anchor(actual.actual_anchor, anchor, budget))
      fail("FieldExitPlacementActualWriteMismatch", actor);
    return out;
  });
}

// 原单次 Common mutation 之后绑定准确 draft；调用方必须携带原 writer 返回
// 的两个身份和 sequence，不能拿当前位置或另一次 capture 的序号补证。
Outcome<BoundSpatialLocationWrite> bind_actual_field_member_loss_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const FieldBattleExitPlacementRead &original, const StableId &actor,
    const MomentOwnerSource &source,
    const std::shared_ptr<const ActorLocationCaptureIdentity> &capture_identity,
    const std::shared_ptr<const ActorLocationWriteIdentity> &write_identity,
    Integer sequence, ExactArithmeticBudget budget) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!original.bound_to(common, movement) || !identity ||
        capture_identity != identity || !write_identity || sequence < 1 ||
        std::size_t(sequence) > drafts.size())
      fail("FieldMemberLossActualCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    if (draft.actor_id != actor || draft.sequence != sequence ||
        draft.write_identity != write_identity ||
        draft.kind != ActorLocationExecutionDraft::Kind::mutation ||
        !draft.before)
      fail("FieldMemberLossActualWriteRequired", actor);
    const auto actual =
        read_field_loss(common, movement, source.source_ref,
                        source.source_schema, actor, &original, budget);
    if (!field_loss_location_matches(actual, draft.after, budget))
      fail("FieldMemberLossActualLocationMismatch", actor);
    BoundSpatialLocationWrite out{BoundSpatialLocationWriteBuildView{
        actor,
        {source.source_ref.domain, source.source_ref.kind,
         source.source_ref.id},
        source.source_schema,
        actual.actual_placement.actual_anchor,
        identity,
        sequence,
        nullptr,
        write_identity}};
    return out;
  });
}

// 热写者须提交实际tap两个身份及序号；完整域source决定真实点/原Army，调用方不能自报锚。
Outcome<BoundSpatialLocationWrite> bind_actual_field_army_exit_stable_location_write(
    const CommonStore &common, const MovementExactState &movement,
    const FieldBattleExitPlacementRead &original, const StableId &actor,
    const MomentOwnerSource &source,
    const std::shared_ptr<const ActorLocationCaptureIdentity> &capture_identity,
    const std::shared_ptr<const ActorLocationWriteIdentity> &write_identity,
    Integer sequence, ExactArithmeticBudget b) {
  return guard<BoundSpatialLocationWrite>([&] {
    const auto identity = common.actor_location_capture_identity();
    const auto &drafts = common.actor_location_execution_drafts();
    if (!original.bound_to(common, movement) || !identity || capture_identity != identity ||
        !write_identity || sequence < 1 || std::size_t(sequence) > drafts.size())
      fail("FieldArmyExitStablePlacementActualCaptureRequired", actor);
    const auto &draft = drafts[std::size_t(sequence - 1)];
    if (draft.actor_id != actor || draft.sequence != sequence || draft.write_identity != write_identity ||
        draft.kind != ActorLocationExecutionDraft::Kind::mutation || !draft.before)
      fail("FieldArmyExitStablePlacementActualWriteRequired", actor);
    const auto actual = read_field_stable(common, movement, source.source_ref,
        source.source_schema, actor, &original, b);
    const auto *in_army = std::get_if<InArmyLocation>(&draft.after);
    const auto *binding = movement.spatial_bindings.find("actor:" + actor);
    const auto *carried = binding ? std::get_if<CarriedMovementWriter>(&binding->writer) : nullptr;
    const auto *army = carried ? std::get_if<ArmyCarrierRef>(&carried->carrier_ref) : nullptr;
    if (!in_army || in_army->army_id != *actual.army_id || !army ||
        army->army_id != *actual.army_id || !same_source(binding->binding_source, source.source_ref))
      fail("FieldArmyExitStablePlacementActualArmyRequired", actor);
    return BoundSpatialLocationWrite{BoundSpatialLocationWriteBuildView{
        actor, {source.source_ref.domain, source.source_ref.kind, source.source_ref.id},
        source.source_schema, actual.actual.actual_anchor, identity, sequence, nullptr, write_identity}};
  });
}

} // namespace hanmo::resident
