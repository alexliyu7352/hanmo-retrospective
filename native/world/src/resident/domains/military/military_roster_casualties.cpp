// 原 settle_casualties 的人产落账体机械提取；旧会战和连续会战共用同一写者。
#include "military_roster_casualties.hpp"
#include "military_model.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 原有向量查找与分类顺序不变，不在公共落账叶猜参战关系。
template <class T> T *find(std::vector<T> &rows, const StableId &id) {
  for (auto &r : rows)
    if (r.id == id)
      return &r;
  return nullptr;
}
template <class T>
const T *pair(const std::vector<std::pair<StableId, T>> &rows,
              const StableId &id) {
  for (const auto &r : rows)
    if (r.first == id)
      return &r.second;
  return nullptr;
}
bool has(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
template <class T> Outcome<T> error(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
template <class T>
std::vector<StableId> keys(const std::vector<std::pair<StableId, T>> &rows) {
  std::vector<StableId> out;
  for (const auto &p : rows)
    out.push_back(p.first);
  std::sort(out.begin(), out.end());
  return out;
}
StableId group(const OriginalMilitaryRosterFinalView &u, const StableId &id) {
  if (has(u.fit, id))
    return "fit";
  if (has(u.wounded, id))
    return "wounded";
  if (has(u.dead, id))
    return "dead";
  if (has(u.captured, id))
    return "captured";
  if (has(u.deserted, id))
    return "deserted";
  return {};
}
} // namespace
Outcome<MilitaryCasualtyFact> apply_original_military_roster_casualties(
    CommonWrite &c, MilitaryState &s, const StableId &actual_battle_source,
    Integer recorded_tick, const std::vector<StableId> &army_ids,
    std::span<const OriginalMilitaryRosterFinalView> finals,
    std::vector<MilitaryActorChange> officers,
    const std::vector<std::pair<StableId, StableId>> &captors,
    bool permits_zero_morale_fit) {
  MilitaryCasualtyFact receipt;
  receipt.battle_id = actual_battle_source;
  receipt.tick = recorded_tick;
  receipt.actor_changes = std::move(officers);
  std::vector<std::pair<StableId, StableId>> new_captors;
  for (const auto &view : finals) {
    const auto &id = view.unit_id;
    auto u = find(s.military_units, id);
    const auto *final = &view;
    auto army = u ? find(s.armies, u->army_id) : nullptr;
    if (!u || !final || !has(army_ids, u->army_id) || !army ||
        army->status != "active")
      return error<MilitaryCasualtyFact>("MilitaryBattleUnitUnavailable");
    if (final->army_id != u->army_id ||
        final->commander_actor_id != u->commander_actor_id ||
        final->troop_type_id != u->troop_type_id ||
        final->member_ids != u->member_ids)
      return error<MilitaryCasualtyFact>("MilitaryBattleRosterMismatch");
    if (final->morale < 0 || final->morale > 100 ||
        (!final->fit.empty() && final->morale == 0 && !permits_zero_morale_fit))
      return error<MilitaryCasualtyFact>("InvalidMilitaryMorale");
    MilitaryRosterReceipt r;
    r.unit_id = id;
    r.fit = final->fit;
    r.wounded = final->wounded;
    r.dead = final->dead;
    r.captured = final->captured;
    r.deserted = final->deserted;
    r.member_ids = u->member_ids;
    r.morale = final->morale;
    r.captors = u->captors;
    for (auto entry : {std::pair<const char *, const std::vector<StableId> *>{
                           "fit", &u->fit},
                       {"wounded", &u->wounded},
                       {"dead", &u->dead},
                       {"captured", &u->captured},
                       {"deserted", &u->deserted}})
      for (auto &person : *entry.second) {
        auto after = group(*final, person);
        if ((has({"dead", "captured", "deserted"}, entry.first) &&
             after != entry.first) ||
            (StableId(entry.first) == "wounded" && after == "fit"))
          return error<MilitaryCasualtyFact>("MilitaryCasualtyReversal");
        if (after == "captured" && StableId(entry.first) != "captured") {
          auto keeper = pair(captors, person);
          auto custodian = keeper ? find(s.armies, *keeper) : nullptr;
          if (!keeper || *keeper == u->army_id || !has(army_ids, *keeper) ||
              !custodian || custodian->status != "active" ||
              custodian->organization_id == army->organization_id)
            return error<MilitaryCasualtyFact>("MilitaryCaptorRequired");
          r.captors.emplace_back(person, *keeper);
          new_captors.emplace_back(person, *keeper);
        }
        if (c.actor(person) && after != entry.first) {
          auto keeper = pair(r.captors, person);
          MilitaryActorChange change;
          change.actor_id = person;
          change.status = after;
          change.army_id = u->army_id;
          change.captor_army_id = keeper ? *keeper : StableId{};
          receipt.actor_changes.push_back(change);
        }
      }
    receipt.units.push_back(r);
  }
  if (keys(captors) != keys(new_captors))
    return error<MilitaryCasualtyFact>("UnexpectedMilitaryCaptor");
  for (auto &r : receipt.units) {
    auto u = find(s.military_units, r.unit_id);
    u->fit = r.fit;
    u->wounded = r.wounded;
    u->dead = r.dead;
    u->captured = r.captured;
    u->deserted = r.deserted;
    u->member_ids = r.member_ids;
    u->morale = r.morale;
    u->captors = r.captors;
  }
  for (auto &p : new_captors)
    find(s.armies, p.second)->prisoner_ids.push_back(p.first);
  for (auto &change : receipt.actor_changes)
    if (has({"dead", "captured", "deserted"}, change.status)) {
      auto army = find(s.armies, change.army_id);
      army->member_actor_ids.erase(std::remove(army->member_actor_ids.begin(),
                                               army->member_actor_ids.end(),
                                               change.actor_id),
                                   army->member_actor_ids.end());
    }
  auto id = c.next_id("military");
  if (!id.ok)
    return {false, {}, id.error};
  receipt.id = id.value;
  auto appended =
      c.append_fact({receipt.id, "military_casualties", "military"});
  if (!appended.ok)
    return {false, {}, appended.error};
  s.casualty_facts.push_back(receipt);
  return {true, std::move(receipt), {}};
}
} // namespace hanmo::resident
