// 按合同队列与原 ID 并列序放置；只返回布局，不发令、不消费随机数或写人物位置。
#include "field_deployment.hpp"
#include "field_integer_geometry.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
Integer field_formation_width_dm(const FieldFormation &formation) {
  return formation.ranks > 0 ? (static_cast<Integer>(formation.fit.size()) +
                                formation.ranks - 1) /
                                   formation.ranks * 15
                             : 0;
}
Integer field_formation_radius_dm(const FieldFormation &formation,
                                  Integer original_fit_count) {
  const __int128_t width =
      formation.ranks > 0
          ? (__int128_t(original_fit_count) + formation.ranks - 1) /
                formation.ranks * 15
          : 0;
  const __int128_t depth = __int128_t(formation.ranks) * 15;
  const auto squared = width * width + depth * depth;
  const Integer lower = field_integer_sqrt(squared);
  return (lower + (__int128_t(lower) * lower < squared ? 1 : 0) + 1) / 2;
}
Integer field_formation_radius_dm(const FieldFormation &formation) {
  return field_formation_radius_dm(formation,
                                   static_cast<Integer>(formation.fit.size()));
}

Outcome<const FieldSideZone *>
read_field_side_geometry(const FieldBattleSession &session,
                         const GeneratedBattleField &field, Integer side) {
  auto fail = []() {
    return Outcome<const FieldSideZone *>{
        false, nullptr, common_failure("FieldPositionOutsideZone").error};
  };
  if (side < 0 || side > 1 || session.sides[side].side != side ||
      session.field_hash != field.field_hash ||
      session.sides[side].deploy_zone.empty())
    return fail();
  const FieldSideZone *found = nullptr;
  for (const auto &zone : field.sides)
    if (zone.deploy_cells == session.sides[side].deploy_zone) {
      if (found)
        return fail();
      found = &zone;
    }
  if (!found)
    return fail();
  return {true, found, {}};
}
namespace {
// 自动旧选择与军议明确选择复用同一原几何；不复制第二套布阵计算。
Outcome<FieldDeploymentLayout>
prepare_layout(const FieldBattleSession &session,
               const GeneratedBattleField &field, Integer side,
               std::optional<FieldAiStrategy> adopted_strategy,
               const std::set<StableId> *original_ids = nullptr) {
  if (side < 0 || side > 1 || session.field_hash != field.field_hash)
    return {false, {}, common_failure("FieldPositionOutsideZone").error};
  // 会话侧号按来军/追者裁定；逆向追击时不等于作者路线两端的数组下标。
  std::array<const FieldSideZone *, 2> zones{};
  for (int s = 0; s < 2; ++s) {
    auto geometry = read_field_side_geometry(session, field, s);
    if (!geometry.ok)
      return {false, {}, geometry.error};
    zones[s] = geometry.value;
  }
  if (zones[0] == zones[1])
    return {false, {}, common_failure("FieldPositionOutsideZone").error};
  const auto fit_count = [&](const FieldFormation &f) -> Integer {
    return original_ids ? f.fit_start : Integer(f.fit.size());
  };
  const auto original_width = [&](const FieldFormation &f) -> Integer {
    return f.ranks > 0 ? (fit_count(f) + f.ranks - 1) / f.ranks * 15 : 0;
  };
  const auto original_radius = [&](const FieldFormation &f) -> Integer {
    const __int128_t w = original_width(f), depth = f.ranks * 15;
    const auto squared = w * w + depth * depth;
    const auto lower = field_integer_sqrt(squared);
    return (lower + (__int128_t(lower) * lower < squared ? 1 : 0) + 1) / 2;
  };
  std::array<Integer, 2> fit{};
  std::array<__int128_t, 2> heights{};
  for (const auto &[id, f] : session.formations) {
    if (original_ids && !original_ids->contains(id))
      continue;
    if (f.side < 0 || f.side > 1 || f.ranks <= 0)
      return {false, {}, common_failure("InvalidFieldFormation", id).error};
    if ((original_ids && fit_count(f) <= 0) ||
        fit[f.side] > std::numeric_limits<Integer>::max() - fit_count(f))
      return {false, {}, common_failure("InvalidFieldFormation", id).error};
    fit[f.side] += fit_count(f);
  }
  if (fit[0] <= 0 || fit[1] <= 0)
    return {false, {}, common_failure("InvalidFieldFormationCount").error};
  for (int s = 0; s < 2; ++s) {
    for (const auto cell : zones[s]->deploy_cells)
      heights[s] += field.cells.at(cell).height_dm;
  }
  const __int128_t count_a = zones[side]->deploy_cells.size(),
                   count_b = zones[1 - side]->deploy_cells.size();
  const bool high_ground =
      heights[side] * count_b - heights[1 - side] * count_a >=
      300 * count_a * count_b;
  if (adopted_strategy && *adopted_strategy != FieldAiStrategy::HoldGround &&
      *adopted_strategy != FieldAiStrategy::LineReserve &&
      *adopted_strategy != FieldAiStrategy::Flanking)
    return {false, {}, common_failure("InvalidFieldCouncilPlan").error};
  FieldDeploymentLayout out;
  if (high_ground || __int128_t(fit[side]) * 10 < __int128_t(fit[1 - side]) * 8)
    out.strategy = FieldAiStrategy::HoldGround;
  else if (__int128_t(fit[side]) * 10 >= __int128_t(fit[1 - side]) * 13)
    out.strategy = FieldAiStrategy::Flanking;
  if (adopted_strategy)
    out.strategy = *adopted_strategy;
  std::vector<const FieldFormation *> infantry, archers, cavalry;
  const FieldFormation *commander = nullptr;
  for (const auto &[id, formation] : session.formations) {
    if (original_ids && !original_ids->contains(id))
      continue;
    if (formation.side != side)
      continue;
    if (formation.commander_actor_id ==
        session.sides[side].commander_actor_id) {
      if (commander)
        return {false,
                {},
                common_failure("FieldCommanderFormationRepeated", id).error};
      commander = &formation;
    }
    if (formation.category == FieldTroopCategory::Infantry)
      infantry.push_back(&formation);
    else if (formation.category == FieldTroopCategory::Archer)
      archers.push_back(&formation);
    else
      cavalry.push_back(&formation);
  }
  if (!commander)
    return {false, {}, common_failure("FieldCommanderFormationRequired").error};
  std::set<StableId> reserves;
  if (out.strategy == FieldAiStrategy::LineReserve) {
    auto weak = infantry;
    std::sort(weak.begin(), weak.end(), [&](auto a, auto b) {
      return fit_count(*a) != fit_count(*b) ? fit_count(*a) < fit_count(*b)
                                            : a->formation_id < b->formation_id;
    });
    for (std::size_t n = 0; n < weak.size() / 4; ++n)
      reserves.insert(weak[n]->formation_id);
  }
  const auto center = zones[side]->deployment_center_dm;
  const auto enemy = zones[1 - side]->road_exit_dm;
  const PointI axis{enemy.x - center.x, enemy.y - center.y};
  const Integer norm =
      field_integer_sqrt(field_distance_squared(enemy, center));
  if (norm <= 0)
    return {false, {}, common_failure("InvalidFieldBattleRoute").error};
  // 布阵相对实际对方出口，而不是把弯曲道路也写死为左右方向。
  const auto desired = [&](Integer forward, Integer across) -> PointI {
    return {center.x + static_cast<Integer>((__int128_t(axis.x) * forward -
                                             __int128_t(axis.y) * across) /
                                            norm),
            center.y + static_cast<Integer>((__int128_t(axis.y) * forward +
                                             __int128_t(axis.x) * across) /
                                            norm)};
  };
  std::vector<std::pair<const FieldFormation *, PointI>> order;
  std::map<StableId, Integer> original_wings;
  order.emplace_back(commander, desired(-2000, 0));
  Integer reserve_offset = original_width(*commander);
  for (const auto *formation : infantry)
    if (formation != commander && reserves.contains(formation->formation_id)) {
      order.emplace_back(formation, desired(-2000, reserve_offset));
      reserve_offset += original_width(*formation);
    }
  for (const auto *group : {&infantry, &archers, &cavalry}) {
    Integer total_width = 0;
    for (const auto *formation : *group)
      if (formation != commander && !reserves.contains(formation->formation_id))
        total_width += original_width(*formation);
    Integer across = -total_width / 2, wing = 0;
    for (const auto *formation : *group) {
      if (formation == commander || reserves.contains(formation->formation_id))
        continue;
      const Integer width = original_width(*formation);
      if (group == &cavalry) {
        // 两翼宽度取实际步卒正面总宽，不额外发明侧翼间隔参数。
        Integer front = 0;
        for (const auto *foot : infantry)
          front += original_width(*foot);
        const Integer offset = front / 2 + width / 2 + (wing / 2) * width;
        const Integer original_wing = wing++ % 2 == 0 ? -1 : 1;
        original_wings.emplace(formation->formation_id, original_wing);
        order.emplace_back(formation, desired(0, original_wing * offset));
      } else {
        const auto lateral = across + width / 2;
        if (lateral)
          original_wings.emplace(formation->formation_id, lateral < 0 ? -1 : 1);
        order.emplace_back(formation,
                           desired(group == &archers ? -600 : 0, lateral));
      }
      across += width;
    }
  }
  std::vector<Integer> radii;
  for (const auto &[formation, target] : order) {
    const Integer radius = original_radius(*formation);
    std::optional<std::size_t> best;
    __int128_t best_distance = 0;
    for (const auto index : zones[side]->deploy_cells) {
      if (!field_cell_passable(field.cells.at(index).terrain))
        continue;
      const auto point = field_cell_center_dm(field, index);
      // 原行位放不下只能向后排；不能因欧氏最近把后备或弓手推到前线之前。
      const __int128_t forward = (__int128_t(point.x) - target.x) * axis.x +
                                 (__int128_t(point.y) - target.y) * axis.y;
      if (forward > 0)
        continue;
      bool occupied = false;
      for (std::size_t n = 0; n < out.placements.size(); ++n)
        if (field_distance_squared(point, out.placements[n].point_dm) <
            __int128_t(radius + radii[n]) * (radius + radii[n])) {
          occupied = true;
          break;
        }
      if (occupied)
        continue;
      const auto distance = field_distance_squared(point, target);
      // §13 cautious 的定案在布阵区取高处；并列才沿原队列几何与格序。
      const bool seek_height =
          adopted_strategy && *adopted_strategy == FieldAiStrategy::HoldGround;
      const bool higher =
          best && field.cells[index].height_dm > field.cells[*best].height_dm;
      const bool same_height =
          !best || field.cells[index].height_dm == field.cells[*best].height_dm;
      if (!best || (seek_height && higher) ||
          ((!seek_height || same_height) &&
           (distance < best_distance ||
            (distance == best_distance && index < *best)))) {
        best = index;
        best_distance = distance;
      }
    }
    if (!best)
      return {false,
              {},
              common_failure("FieldDeploymentNoSpace", formation->formation_id)
                  .error};
    const auto point = field_cell_center_dm(field, *best);
    out.placements.push_back(
        {formation->formation_id, point,
         field_direction_mdeg({enemy.x - point.x, enemy.y - point.y}),
         reserves.contains(formation->formation_id), formation == commander,
         original_wings.contains(formation->formation_id)
             ? original_wings.at(formation->formation_id)
             : 0});
    radii.push_back(radius);
  }
  return {true, std::move(out), {}};
}
} // namespace
// 未经军议覆盖的原调用仍保持原兵力比/地形选择，几何和次序不变。
Outcome<FieldDeploymentLayout>
prepare_field_deployment_layout(const FieldBattleSession &session,
                                const GeneratedBattleField &field,
                                Integer side) {
  return prepare_layout(session, field, side, {});
}
// 明确采纳的布局只替换选择，不修改原编队、任务或派令状态。
Outcome<FieldDeploymentLayout>
prepare_field_deployment_layout(const FieldBattleSession &session,
                                const GeneratedBattleField &field, Integer side,
                                FieldAiStrategy strategy) {
  return prepare_layout(session, field, side, strategy);
}
Outcome<FieldDeploymentLayout> prepare_field_original_deployment_layout(
    const FieldBattleSession &session, const GeneratedBattleField &field,
    Integer side, FieldAiStrategy strategy,
    const std::set<StableId> &original_ids) {
  if (original_ids.empty())
    return {false,
            {},
            common_failure("FieldOriginalFormationRosterRequired").error};
  for (const auto &id : original_ids)
    if (!session.formations.contains(id))
      return {false,
              {},
              common_failure("FieldOriginalFormationRosterRequired", id).error};
  // Caller verifies these exact initial binding IDs and fit_start against the
  // original formation/roster source. Never use today's reduced fit widths.
  return prepare_layout(session, field, side, strategy, &original_ids);
}
} // namespace hanmo::resident
