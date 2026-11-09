#include "resident/model/common_model.hpp"
#include "field_flank.hpp"
#include "field_contacts.hpp"
#include "field_integer_geometry.hpp"
#include "field_navigation.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
Outcome<FieldMoveTo>
read_field_flank_target(const FieldBattleSession &session,
                        const GeneratedBattleField &field, Integer side,
                        const FieldDeploymentPlacement &placement) {
  auto fail = [&](const char *code) -> Outcome<FieldMoveTo> {
    return {false, {}, common_failure(code, placement.formation_id).error};
  };
  auto own = read_field_side_geometry(session, field, side);
  auto enemy = read_field_side_geometry(session, field, 1 - side);
  if (!own.ok)
    return {false, {}, own.error};
  if (!enemy.ok)
    return {false, {}, enemy.error};
  auto formation = session.formations.find(placement.formation_id);
  if (formation == session.formations.end() || formation->second.side != side ||
      static_cast<Integer>(formation->second.category) < 0 ||
      static_cast<Integer>(formation->second.category) > 2 ||
      placement.commander || placement.wing < -1 || placement.wing > 1 ||
      (placement.wing == 0 &&
       formation->second.category == FieldTroopCategory::Cavalry))
    return fail("FieldFlankOriginalWingRequired");
  if (placement.wing == 0) {
    auto left_placement = placement, right_placement = placement;
    // Original layout across-positive is the left normal (-forward.y,
    // forward.x) when facing the actual opposing E_s. Never use today's
    // moved formation position to decide this original centered assignment.
    left_placement.wing = 1;
    right_placement.wing = -1;
    auto left = read_field_flank_target(session, field, side, left_placement);
    if (!left.ok)
      return left;
    auto right = read_field_flank_target(session, field, side, right_placement);
    if (!right.ok)
      return right;
    return field_distance_squared(placement.point_dm, left.value.point_dm) <=
                   field_distance_squared(placement.point_dm,
                                          right.value.point_dm)
               ? left
               : right;
  }
  const auto axis = enemy.value->deployment_axis_dm;
  const Integer norm = field_integer_sqrt(field_distance_squared(axis, {0, 0}));
  const PointI forward{
      enemy.value->road_exit_dm.x - own.value->deployment_center_dm.x,
      enemy.value->road_exit_dm.y - own.value->deployment_center_dm.y};
  if (norm <= 0)
    return fail("FieldFlankOriginalRectangleRequired");
  using Wide = __int128_t;
  const auto away = Wide(enemy.value->deployment_center_dm.x -
                         own.value->deployment_center_dm.x) *
                        axis.x +
                    Wide(enemy.value->deployment_center_dm.y -
                         own.value->deployment_center_dm.y) *
                        axis.y;
  // Own wing is preserved by the original cavalry ordering even if terrain
  // forced its actual placement backwards. Project that side into enemy axes.
  const PointI wing_vector{-forward.y * placement.wing,
                           forward.x * placement.wing};
  const auto across =
      Wide(wing_vector.x) * (-axis.y) + Wide(wing_vector.y) * axis.x;
  if (!away || !across)
    return fail("FieldFlankOriginalRectangleDirectionRequired");
  const Integer length = field.parameters.deployment_length_m * 5 - 1000;
  const Integer width = field.parameters.deployment_width_m * 5 - 1000;
  if (length < 0 || width < 0)
    return fail("FieldFlankRectangleInsetRequired");
  const Integer along_sign = away > 0 ? 1 : -1,
                across_sign = across > 0 ? 1 : -1;
  const Wide dx = (Wide(axis.x) * length * along_sign -
                   Wide(axis.y) * width * across_sign) /
                  norm;
  const Wide dy = (Wide(axis.y) * length * along_sign +
                   Wide(axis.x) * width * across_sign) /
                  norm;
  const Wide tx = Wide(enemy.value->deployment_center_dm.x) + dx;
  const Wide ty = Wide(enemy.value->deployment_center_dm.y) + dy;
  if (tx < std::numeric_limits<Integer>::min() ||
      tx > std::numeric_limits<Integer>::max() ||
      ty < std::numeric_limits<Integer>::min() ||
      ty > std::numeric_limits<Integer>::max())
    return fail("FieldFlankGeometryOverflow");
  PointI target{Integer(tx), Integer(ty)};
  const auto &p = field.parameters;
  const auto passable = [&](std::size_t i) {
    const auto terrain = std::size_t(field.cells[i].terrain);
    return terrain < p.terrain_speed_bp.size() &&
           p.terrain_speed_bp[terrain]
                             [std::size_t(formation->second.category)] > 0;
  };
  bool can_enter = false;
  if (p.cell_m > 0 && target.x >= -p.length_m * 5 &&
      target.x < p.length_m * 5 && target.y >= -p.width_m * 5 &&
      target.y < p.width_m * 5) {
    auto i = std::size_t((target.y + p.width_m * 5) / (p.cell_m * 10) *
                             field.columns +
                         (target.x + p.length_m * 5) / (p.cell_m * 10));
    can_enter = i < field.cells.size() && passable(i);
  }
  if (!can_enter) {
    std::optional<std::size_t> best;
    Wide distance = 0;
    for (std::size_t i = 0; i < field.cells.size(); ++i)
      if (passable(i)) {
        auto d = field_distance_squared(target, field_cell_center_dm(field, i));
        if (!best || d < distance || (d == distance && i < *best)) {
          best = i;
          distance = d;
        }
      }
    if (!best)
      return fail("FieldFlankOriginalCategoryCellRequired");
    target = field_cell_center_dm(field, *best);
  }
  return {true,
          {target, field_direction_mdeg(
                       {enemy.value->deployment_center_dm.x - target.x,
                        enemy.value->deployment_center_dm.y - target.y})},
          {}};
}
Outcome<FieldFlankFollowupRead>
read_field_flank_followup(const FieldBattleSession &session,
                          const GeneratedBattleField &field, const StableId &id,
                          Integer index, const FieldAttackPrefixRead *prefix) {
  auto fail = [&]() -> Outcome<FieldFlankFollowupRead> {
    return {false,
            {},
            common_failure("FieldOriginalFlankContinuationRequired", id).error};
  };
  auto found = session.formations.find(id);
  if (found == session.formations.end() || found->second.side < 0 ||
      found->second.side > 1 || index < 0)
    return fail();
  const auto &formation = found->second;
  const auto &council = session.councils[formation.side];
  if (!council.closed || !council.adopted_proposal_id)
    return fail();
  const FieldCouncilProposal *proposal = nullptr;
  for (const auto &p : council.proposals)
    if (p.proposal_id == *council.adopted_proposal_id) {
      if (proposal)
        return fail();
      proposal = &p;
    }
  if (!proposal || std::size_t(index) >= proposal->plan.assignments.size())
    return fail();
  const auto &assignment = proposal->plan.assignments[std::size_t(index)];
  const auto move = std::get_if<FieldMoveTo>(&assignment.mission);
  if (!assignment.flank_followup || assignment.formation_id != id || !move)
    return fail();
  const auto position = [&](const FieldFormation &f) -> PointI {
    if (!prefix)
      return f.position_dm;
    return prefix->motion.formations.at(f.formation_id).position_dm;
  };
  if (prefix) {
    for (const auto &[key, f] : session.formations)
      if (!prefix->motion.formations.contains(key))
        return fail();
  }
  const auto present = [&](const FieldFormation &f) {
    if (!prefix)
      return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
             f.state != FieldFormationState::Destroyed;
    const auto &p = prefix->motion.formations.at(f.formation_id);
    return p.fit > 0 && !p.withdrawn;
  };
  const auto at = position(formation);
  if (at.x != move->point_dm.x || at.y != move->point_dm.y)
    return fail();
  const FieldFormation *target = nullptr;
  __int128_t best = 0;
  const auto consider = [&](const FieldFormation &f) {
    const auto distance = field_distance_squared(at, position(f));
    if (!target || distance < best ||
        (distance == best && f.formation_id < target->formation_id)) {
      target = &f;
      best = distance;
    }
  };
  for (const auto &[key, f] : session.formations)
    if (f.side != formation.side && f.category == FieldTroopCategory::Archer &&
        present(f))
      consider(f);
  if (!target) {
    auto contacts =
        read_field_attack_contacts_at_prefix(session, field, prefix);
    if (!contacts.ok)
      return {false, {}, contacts.error};
    for (const auto &[key, f] : session.formations)
      if (f.side != formation.side && present(f) &&
          std::find(contacts.value.melee_formations.begin(),
                    contacts.value.melee_formations.end(),
                    key) != contacts.value.melee_formations.end())
        consider(f);
  }
  return {true, {&assignment, target, at, move->facing_mdeg}, {}};
}
} // namespace hanmo::resident
