#include "actor_spatial_read.hpp"
#include "local_move_source.hpp"
#include "resident/storage/common_store.hpp"
#include "military_action_placement.hpp"
#include "movement_control.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <initializer_list>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
void retain(ActorSpatialLocationView &out, const SourceRef &ref,
            Integer schema) {
  for (const auto &prior : out.actual_location_sources)
    if (same_source(prior.source, ref)) {
      if (prior.schema != schema)
        fail("ActorSpatialSourceSchemaConflict", ref.id);
      return;
    }
  out.actual_location_sources.push_back({ref, schema});
}
Integer binding_schema(const SourceRef &ref) {
  if (is_military_action_placement_source_kind(ref) ||
      (ref.domain == "military" && ref.kind == "military_spatial_handoff"))
    return 2;
  if (ref.domain == "travel" && ref.kind == "travel_dependent_binding_change")
    return 2;
  if (ref.domain == "family" && ref.kind == "actor_creation")
    return 2;
  return 1;
}
template <class Visitor>
bool visit_actual_transport_participation(const SpatialCarrierState &state,
                                          const SpatialCarrierSources &sources,
                                          const StableId &actor,
                                          Visitor &&visitor) {
  bool found = false;
  for (const auto &row : state.transport_units.rows()) {
    const auto &unit = row.second;
    if (unit.state == TransportUnitLifecycle::dissolved ||
        std::find(unit.named_participants.begin(),
                  unit.named_participants.end(),
                  actor) == unit.named_participants.end())
      continue;
    take(sources.verify_registration_source(unit));
    if (found)
      fail("ActorInMultipleTransportUnits", actor);
    visitor(unit);
    found = true;
  }
  return found;
}
} // namespace

const MovementAnchor *
ordinary_actor_movement_anchor(const ActorSpatialLocationView &v) noexcept {
  return std::get_if<MovementAnchor>(&v.actual_anchor);
}
Outcome<const MovementAnchor *>
require_actor_ordinary_movement_anchor(const ActorSpatialLocationView &v) {
  return guard<const MovementAnchor *>([&] {
    const auto *anchor = ordinary_actor_movement_anchor(v);
    if (!anchor)
      fail("ActorOrdinaryMovementAnchorRequired", v.actor_id);
    return anchor;
  });
}

Outcome<ActorSpatialLocationView> read_actual_actor_spatial_location(
    const CommonRead &common, const MovementExactState &movement,
    const SpatialCarrierSources &original, ActorBattleSpatialSources battles,
    const StableId &actor_id, ExactArithmeticBudget b) {
  return guard<ActorSpatialLocationView>([&] {
    const auto *actor = common.actor(actor_id);
    if (!actor)
      fail("SpatialActorRequired", actor_id);
    ActorSpatialLocationView out;
    out.actor_id = actor_id;
    const auto *binding = [&]() -> const SpatialWriterBinding * {
      const std::initializer_list<std::string_view> pieces{"actor:", actor_id};
      const auto key_memory = [&]() -> CheckpointMemoryCharge {
        if (checkpoint_boundary_decode_active())
          return CheckpointMemoryCharge{};
        std::size_t count = 0;
        for (auto piece : pieces) {
          if (piece.size() > std::numeric_limits<std::size_t>::max() - count)
            throw std::bad_alloc();
          count += piece.size();
        }


        return CheckpointMemoryCharge(0);
      }();
      const auto key = checkpoint_decode_boundary_join(pieces);
      return movement.spatial_bindings.find(key);
    }();
    // Common 的真实绑定先于冻结 Location；空 provider 不是“未在战”。
    const bool field_bound = take(common.field_battle_bound(actor_id));
    if (battles.field) {
      if (!battles.field->bound_to(common, movement))
        fail("CrossCandidateFieldBattleActorSpatialRead", actor_id);
      auto field = take(battles.field->read_field_actor_location(actor_id));
      if (field.has_value() != field_bound)
        fail("FieldBattleActorSpatialBindingMismatch", actor_id);
      if (field) {
        const auto *point =
            std::get_if<FieldBattleSpatialView>(&field->actual_anchor);
        if (field->actor_id != actor_id || !point || point->battle_id.empty() ||
            point->army_id.empty() || point->formation_id.empty() ||
            point->field_hash.empty())
          fail("FieldBattleActorSpatialSourceRequired", actor_id);
        bool session_source = false, contact_source = false;
        for (const auto &source : field->actual_location_sources) {
          if (!source_valid(source.source) || source.schema != 1 ||
              source.source.domain != "military")
            fail("FieldBattleActorSpatialSourceRequired", actor_id);
          if (source.source.kind == "field_battle_session" &&
              source.source.id == point->battle_id)
            session_source = true;
          else if (source.source.kind == "field_battle_contact")
            contact_source = true;
          else
            fail("FieldBattleActorSpatialSourceRequired", actor_id);
        }
        if (!session_source || !contact_source)
          fail("FieldBattleActorSpatialSourceRequired", actor_id);
        if (binding) {
          bool legacy_writer =
              std::holds_alternative<BattleMovementWriter>(binding->writer);
          if (const auto *carried =
                  std::get_if<CarriedMovementWriter>(&binding->writer))
            legacy_writer = std::holds_alternative<BattleMovementWriter>(
                take(read_resolved_spatial_writer(movement,
                                                  carried->carrier_ref))
                    .writer);
          if (legacy_writer)
            fail("ActorBattleSpatialAuthorityConflict", actor_id);
        }
        return std::move(*field);
      }
    } else if (field_bound)
      fail("FieldBattleActorSpatialReadRequired", actor_id);
    // 仅实际旧 Battle writer 才要求旧读口；普通位置不靠空旧接口造成功。
    const auto read_legacy = [&](const BattleMovementWriter &writer) {
      if (!battles.legacy)
        fail("BattleActorSpatialReadRequired", actor_id);
      if (!battles.legacy->bound_to(common, movement))
        fail("CrossCandidateBattleActorSpatialRead", actor_id);
      auto result =
          take(battles.legacy->read_battle_actor_location(actor_id, writer));
      if (result.actor_id != actor_id ||
          result.actual_location_sources.empty() ||
          !std::holds_alternative<ActualMilitaryActorPosition>(
              result.actual_anchor))
        fail("BattleActorSpatialSourceRequired", actor_id);
      return result;
    };
    if (!binding) {
      out.actual_anchor = take(original.original_actor_anchor(actor_id));
      const auto &source = std::visit(
          [](const auto &x) -> const SourceRef & {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, TerrainContinuousAnchor>)
              return x.actual_prefix_close_source;
            else
              return x.actual_source;
          },
          *take(require_actor_ordinary_movement_anchor(out)));
      if (!source_valid(source))
        fail("InitialActorSpatialOriginRequired", actor_id);
      out.actual_location_sources.push_back(
          {source, source.domain == "family" && source.kind == "actor_creation"
                       ? 2
                       : 1});
    } else if (const auto *carried =
                   std::get_if<CarriedMovementWriter>(&binding->writer)) {
      auto resolved =
          take(read_resolved_spatial_writer(movement, carried->carrier_ref));
      if (const auto *local =
              std::get_if<BattleMovementWriter>(&resolved.writer)) {
        out = read_legacy(*local);
      } else
        out.actual_anchor = take(
            read_carrier_movement_anchor(movement, carried->carrier_ref, b));
      retain(out, binding->binding_source,
             binding_schema(binding->binding_source));
      retain(out, resolved.binding_source,
             binding_schema(resolved.binding_source));
      if (const auto *world =
              std::get_if<WorldMovementWriter>(&resolved.writer))
        out.actual_location_sources.push_back(
            {{"travel", "movement_prefix", world->prefix_id}, 1});
    } else if (const auto *local =
                   std::get_if<BattleMovementWriter>(&binding->writer)) {
      out = read_legacy(*local);
      return out;
    } else {
      if (const auto *world =
              std::get_if<WorldMovementWriter>(&binding->writer)) {
        const auto *prefix = movement.prefixes.find(world->prefix_id);
        if (!prefix)
          fail("ActorSpatialPrefixRequired", actor_id);
        out.actual_anchor = take(read_movement_position(*prefix, b));
        const auto head = take(read_personal_movement_immutable_head(
            common, movement, *prefix, actor_id, b));
        out.actual_location_sources.push_back(
            {head.source_ref, head.source_schema});
      } else {
        out.actual_anchor =
            std::get<StableMovementWriter>(binding->writer).anchor;
        if(binding->binding_source.kind=="local_move") {
          const auto* store=dynamic_cast<const CommonStore*>(&common);
          if(!store) fail("LocalMoveOriginalCommonRequired",actor_id);
          const auto* row=take(read_local_move_source(*store,movement,binding->binding_source,1,b));
          if(row->actor_id!=actor_id || !take(same_spatial_anchor(
              std::get<StableMovementWriter>(binding->writer).anchor,
              MovementAnchor{OriginalMovementAnchor{row->to,binding->binding_source}},b)))
            fail("LocalMovePhysicalBindingMismatch",actor_id);
        }
        if (!source_valid(binding->binding_source))
          fail("ActorSpatialBindingSourceRequired", actor_id);
        out.actual_location_sources.push_back(
            {binding->binding_source, binding_schema(binding->binding_source)});
      }
    }
    if (binding &&
        std::holds_alternative<CarriedMovementWriter>(binding->writer)) {
      const auto &carried = std::get<CarriedMovementWriter>(binding->writer);
      const auto *unit =
          std::get_if<TransportUnitCarrierRef>(&carried.carrier_ref);
      const auto *location = std::get_if<CarriedLocation>(&actor->location);
      const auto *army = std::get_if<ArmyCarrierRef>(&carried.carrier_ref);
      const auto *in_army = std::get_if<InArmyLocation>(&actor->location);
      const bool valid =
          army ? in_army && in_army->army_id == army->army_id
               : unit && location &&
                     location->carrier_kind ==
                         CarriedLocation::CarrierKind::transport_unit &&
                     location->carrier_id ==
                         unit->transport_unit_ref.transport_unit_id;
      if (!valid)
        fail("ActorCarriedLocationMismatch", actor_id);
      out.location_snapshot = actor->location;
      return out;
    }
    out.location_snapshot = take(project_movement_location(
        *take(require_actor_ordinary_movement_anchor(out)), b));
    {
      static_assert(
          std::is_nothrow_move_constructible_v<OriginalMovementAnchor>,
          "Actor spatial projection comparison requires nonthrowing "
          "original-anchor transfer");


      const auto copied_inputs = CheckpointMemoryCharge(0);
      auto same = take(same_spatial_anchor(
          OriginalMovementAnchor{actor->location,
                                 {"common", "actor_projection", actor_id}},
          OriginalMovementAnchor{out.location_snapshot,
                                 {"common", "actor_projection", actor_id}},
          b));
      if (!same)
        fail("ActorSpatialProjectionMismatch", actor_id);
    }
    return out;
  });
}
// 原引用重载不另走一套规则，必须经过真实 field 绑定缺失检查。
Outcome<ActorSpatialLocationView> read_actual_actor_spatial_location(
    const CommonRead &common, const MovementExactState &movement,
    const SpatialCarrierSources &original, const BattleActorSpatialRead &battle,
    const StableId &actor_id, ExactArithmeticBudget b) {
  return read_actual_actor_spatial_location(common, movement, original,
                                            ActorBattleSpatialSources{battle},
                                            actor_id, b);
}
Outcome<std::optional<ActorTransportParticipation>>
read_actual_transport_participation(const SpatialCarrierState &state,
                                    const SpatialCarrierSources &sources,
                                    const StableId &actor) {
  return guard<std::optional<ActorTransportParticipation>>(
      [&]() -> std::optional<ActorTransportParticipation> {
        std::optional<ActorTransportParticipation> result;
        visit_actual_transport_participation(
            state, sources, actor, [&](const auto &unit) {
              result = ActorTransportParticipation{
                  {unit.subject, unit.transport_unit_id},
                  unit.composition_revision,
                  unit.registration_source_ref,
                  unit.active_journey_id};
            });
        return result;
      });
}
Outcome<bool> read_actual_transport_participation_presence(
    const SpatialCarrierState &state, const SpatialCarrierSources &sources,
    const StableId &actor) {
  return guard<bool>([&] {
    return visit_actual_transport_participation(state, sources, actor,
                                                [](const auto &) {});
  });
}
} // namespace hanmo::resident
