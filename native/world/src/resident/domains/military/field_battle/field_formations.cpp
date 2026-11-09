#include "resident/domains/military/military_model.hpp"
// 成军出处继续走原 reader；本叶仅映射连续编队，不创建或修改旧 BattleUnit。
#include "field_formations.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 私有查找防御重复 ID，不能任选第一条构造另一个来源。
template <class T>
const T *unique_record(const std::vector<T> &rows, const StableId &id) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (found)
        return nullptr;
      found = &row;
    }
  return found;
}
// 五种状态必须是原 member_ids 的不相交完备分割，不能通过总数相等掩盖换人。
bool original_roster(const MilitaryUnitRecord &unit) {
  std::set<StableId> original(unit.member_ids.begin(), unit.member_ids.end());
  if (original.size() != unit.member_ids.size() || original.contains(""))
    return false;
  std::set<StableId> actual;
  for (const auto *group :
       {&unit.fit, &unit.wounded, &unit.dead, &unit.captured, &unit.deserted})
    for (const auto &id : *group)
      if (!original.contains(id) || !actual.insert(id).second)
        return false;
  return actual == original;
}
// 不为未知类别猜一个步卒默认，作者兵种标签须可由合同三类明确解释。
Outcome<FieldTroopCategory> category(const StableId &value) {
  if (value == "infantry")
    return {true, FieldTroopCategory::Infantry, {}};
  if (value == "archer")
    return {true, FieldTroopCategory::Archer, {}};
  if (value == "cavalry")
    return {true, FieldTroopCategory::Cavalry, {}};
  return {false, {}, common_failure("InvalidFieldTroopCategory", value).error};
}
} // namespace
Outcome<std::vector<FieldFormation>> read_field_army_formations(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const StableId &army_id, Integer side,
    const StableId &player, const MilitaryFormationSourceRead *exact) {
  if (exact)
    return {
        false,
        {},
        common_failure("FieldFormationActualMomentRequired", army_id).error};
  auto now = normalize_world_moment(common.tick(), {});
  if (!now.ok)
    return {false, {}, now.error};
  return read_field_army_formations(common, military, catalog, army_id, side,
                                    player, now.value, nullptr);
}
static Outcome<std::vector<FieldFormation>> read_field_army_formations_impl(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const StableId &army_id, Integer side,
    const StableId &player, const WorldMoment &actual_now,
    const MilitaryFormationSourceRead *exact,
    CheckpointMemoryCharge *retained_output = nullptr,
    const StableId *selected_unit = nullptr) {
  auto encoded = encode_world_moment(actual_now);
  if (!encoded.ok)
    return {false, {}, encoded.error};
  if (side < 0 || side > 1)
    return {false, {}, common_failure("InvalidFieldBattleSide", army_id).error};
  const auto *army = unique_record(military.armies, army_id);
  if (!army || army->status != "active" || army->unit_ids.empty())
    return {
        false, {}, common_failure("MilitaryArmyUnavailable", army_id).error};
  const auto formed = read_original_military_formation(
      common.state(), military, *army, actual_now, exact);
  if (!formed.ok)
    return {false, {}, formed.error};
  const auto *commander = common.actor(army->commander_actor_id);
  if (!commander || !commander->alive || !commander->available)
    return {
        false,
        {},
        common_failure("MilitaryCommanderUnavailable", army->commander_actor_id)
            .error};
  std::vector<FieldFormation> result;
  if (retained_output) {

    std::size_t count = 0;
    for (const auto &id : army->unit_ids) {
      if (selected_unit && id != *selected_unit)
        continue;
      const auto *unit = unique_record(military.military_units, id);
      if (!unit || unit->fit.empty() || unit->morale <= 0)
        continue; // original validation below still determines the first error
      ++count;


    }

    (*retained_output).grow(0);
    result.reserve(count);
  }
  std::set<StableId> units, members;
  for (const auto &id : army->unit_ids) {
    const auto *unit = unique_record(military.military_units, id);
    if (!units.insert(id).second || !unit || unit->army_id != army_id ||
        unit->organization_id != army->organization_id ||
        !original_roster(*unit))
      return {
          false, {}, common_failure("MilitaryActualRosterMissing", id).error};
    for (const auto &member : unit->member_ids)
      if (!members.insert(member).second)
        return {
            false, {}, common_failure("MilitaryMemberRepeated", member).error};
    if (selected_unit && id != *selected_unit)
      continue;
    if (unit->fit.empty() || unit->morale <= 0)
      continue;
    if (unit->morale > 100 || !unit->mobilizing_action_id.empty() ||
        (unit->training_action_id && !unit->training_action_id->empty()))
      return {false, {}, common_failure("MilitaryUnitReserved", id).error};
    const auto *troop =
        unique_record(catalog.battle_catalog.troop_types, unit->troop_type_id);
    const auto *officer = common.actor(unit->commander_actor_id);
    if (!troop || !officer || !officer->alive || !officer->available)
      return {
          false, {}, common_failure("MilitaryCommanderUnavailable", id).error};
    auto kind = category(troop->category);
    if (!kind.ok)
      return {false, {}, kind.error};
    FieldFormation f;
    f.formation_id = unit->id;
    f.army_id = army_id;
    f.troop_type_id = unit->troop_type_id;
    f.commander_actor_id = unit->commander_actor_id;
    f.side = side;
    f.category = kind.value;
    f.ranks = f.category == FieldTroopCategory::Infantry ? 6
              : f.category == FieldTroopCategory::Archer ? 4
                                                         : 3;
    f.fit = unit->fit;
    f.wounded = unit->wounded;
    f.dead = unit->dead;
    f.captured = unit->captured;
    f.deserted = unit->deserted;
    f.captors = unit->captors;
    f.fit_start = static_cast<Integer>(unit->fit.size());
    f.morale = f.morale_start = unit->morale;
    if (f.commander_actor_id == player ||
        f.commander_actor_id == army->commander_actor_id)
      f.control = FieldDirectControl{f.commander_actor_id};
    result.push_back(std::move(f));
  }
  if (result.empty())
    return {
        false, {}, common_failure("InvalidFieldFormationCount", army_id).error};
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.formation_id < b.formation_id;
  });
  return {true, std::move(result), {}};
}
Outcome<std::vector<FieldFormation>> read_field_army_formations(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const StableId &army_id, Integer side,
    const StableId &player, const WorldMoment &actual_now,
    const MilitaryFormationSourceRead *exact) {
  if (actual_now.half_day_tick != common.tick())
    return {
        false,
        {},
        common_failure("FieldFormationActualMomentMismatch", army_id).error};
  return read_field_army_formations_impl(common, military, catalog, army_id,
                                         side, player, actual_now, exact);
}
Outcome<std::vector<FieldFormation>>
read_field_army_formations_at_original_clock(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &field,
    const StableId &army_id, Integer side, const StableId &player,
    const FieldSegmentApplicationRead &clock,
    const MilitaryFormationSourceRead &exact,
    CheckpointMemoryCharge *retained_output) {
  if (!clock.bound_to(common, military, field) ||
      !exact.bound_to(common.state(), military))
    return {false,
            {},
            common_failure("CrossCandidateFieldFormationClock", army_id).error};
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  return read_field_army_formations_impl(common, military, catalog, army_id,
                                         side, player, now.value, &exact,
                                         retained_output);
}
Outcome<FieldFormation> read_field_reinforcement_formation_at_original_clock(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &field,
    const StableId &army_id, const StableId &formation_id, Integer side,
    const StableId &player, const FieldSegmentApplicationRead &clock,
    const MilitaryFormationSourceRead &exact,
    CheckpointMemoryCharge &retained_output) {
  if (!clock.bound_to(common, military, field) ||
      !exact.bound_to(common.state(), military) || formation_id.empty())
    return {false,
            {},
            common_failure("CrossCandidateFieldFormationClock", army_id).error};
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto rows = read_field_army_formations_impl(
      common, military, catalog, army_id, side, player, now.value, &exact,
      &retained_output, &formation_id);
  if (!rows.ok)
    return {false, {}, rows.error};
  if (rows.value.size() != 1 || rows.value.front().formation_id != formation_id)
    return {
        false,
        {},
        common_failure("FieldReinforcementOriginalUnitRequired", formation_id)
            .error};
  return {true, std::move(rows.value.front()), {}};
}
} // namespace hanmo::resident
