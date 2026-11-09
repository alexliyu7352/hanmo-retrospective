// 原连续会战段提交：只在同一未发布候选中复用原名册写者，不构造旧六角会战。
#include "field_author_data.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/military_roster_casualties.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 任何失败均交外层整 World 候选丢弃；此叶不自行提交局部副本。
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (found)
        fail("FieldSegmentDuplicateOriginalPayload", id);
      found = &row;
    }
  return found;
}
bool has(const std::vector<StableId> &rows, const StableId &id) {
  return std::find(rows.begin(), rows.end(), id) != rows.end();
}
bool same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
bool no_later(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) <= std::tie(b.tick, b.t);
}
// 只核原五类完整互斥与原成员身份，禁止删员后用人数补齐。
void original_partition(const FieldFormation &formation,
                        const MilitaryUnitRecord &unit,
                        std::set<StableId> &battle_members) {
  std::set<StableId> actual;
  for (const auto *group : {&formation.fit, &formation.wounded, &formation.dead,
                            &formation.captured, &formation.deserted})
    for (const auto &id : *group)
      if (id.empty() || !actual.insert(id).second ||
          !battle_members.insert(id).second)
        fail("FieldSegmentRosterPartitionMismatch", formation.formation_id);
  const std::set<StableId> expected(unit.member_ids.begin(),
                                    unit.member_ids.end());
  if (expected.size() != unit.member_ids.size() || expected != actual)
    fail("FieldSegmentRosterPartitionMismatch", formation.formation_id);
}
// 将领只读本场实际 exposure 记录；已编入原名册的人由唯一名册落账，不再计一次。
std::vector<MilitaryActorChange>
officer_changes(const CommonStore &common, const MilitaryState &military,
                const FieldBattleSession &session) {
  std::map<StableId, Integer> prior;
  for (const auto &segment : session.settlements)
    for (const auto &id : segment.casualty_fact_ids) {
      const auto *receipt = unique(military.casualty_facts, id);
      if (!receipt)
        fail("FieldSegmentCasualtySourceRequired", id);
      for (const auto &change : receipt->actor_changes)
        if (change.status == "dead" || change.status == "wounded")
          prior[change.actor_id] =
              std::max(prior[change.actor_id],
                       change.status == "dead" ? Integer{2} : Integer{1});
    }
  std::vector<MilitaryActorChange> out;
  std::set<StableId> changed;
  for (const auto &[id, formation] : session.formations) {
    const auto *unit = unique(military.military_units, id);
    const auto *army = unit ? unique(military.armies, unit->army_id) : nullptr;
    if (!unit || !army)
      fail("FieldSegmentOriginalArmyRequired", id);
    std::map<StableId, Integer> actual;
    std::map<StableId, FieldBattleTime> previous_at;
    for (const auto &loss : formation.commander_casualties) {
      if ((loss.actor_id != formation.commander_actor_id &&
           formation.deputy_actor_id != loss.actor_id) ||
          loss.loss < 1 || loss.loss > 2 ||
          loss.loss <= actual[loss.actor_id] ||
          !no_later(formation.joined_at, loss.at) ||
          !no_later(loss.at, session.now) ||
          (previous_at.contains(loss.actor_id) &&
           !no_later(previous_at.at(loss.actor_id), loss.at)))
        fail("FieldSegmentCommanderSourceMismatch", id);
      actual[loss.actor_id] = loss.loss;
      previous_at[loss.actor_id] = loss.at;
    }
    for (const auto &[actor, loss] : actual) {
      if (has(unit->member_ids, actor))
        continue;
      if (loss < prior[actor])
        fail("FieldSegmentCommanderLossReversed", actor);
      if (loss == prior[actor])
        continue;
      const auto *person = common.actor(actor);
      if (!person || !person->alive || !person->available ||
          !has(army->member_actor_ids, actor) || !changed.insert(actor).second)
        fail("FieldSegmentCommanderSourceMismatch", actor);
      MilitaryActorChange change;
      change.actor_id = actor;
      change.army_id = army->id;
      change.status = loss == 2 ? "dead" : "wounded";
      out.push_back(std::move(change));
    }
  }
  return out;
}
} // namespace
Outcome<MilitaryBattleStep> settle_field_battle_segment_in_candidate(
    CommonStore &common, MilitaryState &military, FieldBattleWorldState &world,
    const FieldBattleCatalog &catalog, const StableId &battle_id,
    const FieldSegmentApplicationRead &actual_clock) {
  try {
    if (!actual_clock.bound_to(common, military, world))
      fail("CrossCandidateFieldSegmentApplicationClock", battle_id);
    const auto applied_at =
        take(actual_clock.read_field_window_closing_moment());
    if (take(compare_world_moments(
            applied_at, take(actual_clock.read_current_moment()))) != 0)
      fail("FieldSegmentApplicationClockMismatch", battle_id);
    take(compare_world_moments(applied_at, applied_at));
    const auto found = world.sessions.find(battle_id);
    if (found == world.sessions.end())
      fail("FieldSegmentSessionRequired", battle_id);
    auto &session = found->second;
    const auto &parameters = catalog.parameters;
    const bool terminal = session.state == FieldBattleState::Ended;
    const auto *head = common.state().facts.find(battle_id);
    if (session.battle_id != battle_id || !head || head->domain != "military" ||
        head->kind != "field_battle_session" || session.created_tick < 0 ||
        session.created_tick > common.tick() || session.segment < 0 ||
        session.segment >= parameters.max_segments ||
        parameters.window_s <= 0 || parameters.step_s <= 0 ||
        parameters.window_s % parameters.step_s || session.now.tick < 0 ||
        session.now.t < 0 || session.now.t >= parameters.window_s ||
        session.now.t % parameters.step_s ||
        (!terminal && !session.window_complete) ||
        (!terminal && session.state != FieldBattleState::Running) ||
        terminal != bool(session.result))
      fail("FieldSegmentActualBoundaryRequired", battle_id);
    if (session.window_complete) {
      if (session.now.t != 0 || session.now.tick != applied_at.half_day_tick)
        fail("FieldSegmentActualWindowRequired", battle_id);
    } else if (static_cast<__int128_t>(session.now.tick) + 1 !=
               applied_at.half_day_tick) {
      fail("FieldSegmentActualWindowRequired", battle_id);
    }
    if (terminal &&
        (!same(session.result->ended, session.now) ||
         session.result->winner_side < -1 || session.result->winner_side > 1 ||
         session.result->reason.empty()))
      fail("FieldSegmentTerminalSourceRequired", battle_id);
    take(validate_field_segment_application_sources(common, military, world,
                                                    actual_clock));
    for (const auto &settled : session.settlements)
      if (settled.segment == session.segment) {
        auto actual = take(read_field_segment_settlement_source(
            common, military, world,
            {"military", "field_battle_segment", settled.fact_id},
            &actual_clock));
        if (!same(settled.settled_at, session.now) ||
            bool(actual.terminal) != terminal)
          fail("FieldSegmentDuplicateBoundaryMismatch", battle_id);
        // 已提交段没有新的业务效果，尤其不能把旧人物伤亡交下游重放。
        return {true, MilitaryBattleStep{}, {}};
      }
    if (session.settlements.size() !=
            static_cast<std::size_t>(session.segment) ||
        session.formations.empty())
      fail("FieldSegmentPreviousSettlementRequired", battle_id);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : world.contacts)
      if (row.battle_id == battle_id) {
        if (contact || id != row.contact_id)
          fail("FieldSegmentContactSourceMismatch", battle_id);
        contact = &row;
      }
    if (!contact)
      fail("FieldSegmentContactSourceRequired", battle_id);
    const auto *contact_head = common.state().facts.find(contact->contact_id);
    if (!contact_head || contact_head->domain != "military" ||
        contact_head->kind != "field_battle_contact")
      fail("FieldSegmentContactSourceRequired", battle_id);
    std::set<StableId> armies, members;
    std::vector<StableId> units;
    std::vector<OriginalMilitaryRosterFinalView> finals;
    std::vector<std::pair<StableId, StableId>> captors;
    for (const auto &[id, formation] : session.formations) {
      const auto *unit = unique(military.military_units, id);
      if (!unit || id != formation.formation_id ||
          unit->army_id != formation.army_id ||
          unit->troop_type_id != formation.troop_type_id ||
          unit->commander_actor_id != formation.commander_actor_id ||
          formation.side < 0 || formation.side > 1 ||
          !no_later(formation.joined_at, session.now))
        fail("FieldSegmentOriginalRosterRequired", id);
      original_partition(formation, *unit, members);
      std::set<StableId> captive_ids;
      for (const auto &[actor, keeper] : formation.captors) {
        if (!has(formation.captured, actor) ||
            !captive_ids.insert(actor).second)
          fail("FieldSegmentCaptorSourceMismatch", id);
        bool enemy = false;
        for (const auto &[other_id, other] : session.formations)
          if (other.army_id == keeper && other.side != formation.side)
            enemy = true;
        if (!enemy)
          fail("FieldSegmentCaptorSourceMismatch", id);
        if (!has(unit->captured, actor))
          captors.emplace_back(actor, keeper);
        else if (std::find(unit->captors.begin(), unit->captors.end(),
                           std::pair<StableId, StableId>{actor, keeper}) ==
                 unit->captors.end())
          fail("FieldSegmentCaptorSourceMismatch", id);
      }
      if (captive_ids != std::set<StableId>(formation.captured.begin(),
                                            formation.captured.end()))
        fail("FieldSegmentCaptorSourceMismatch", id);
      armies.insert(formation.army_id);
      units.push_back(id);
      finals.push_back({unit->id, unit->army_id, unit->commander_actor_id,
                        unit->troop_type_id, unit->member_ids, formation.fit,
                        formation.wounded, formation.dead, formation.captured,
                        formation.deserted, formation.morale});
    }
    auto officers = officer_changes(common, military, session);
    FieldSegmentSettlement settlement;
    settlement.fact_id = take(common.next_id("field_segment"));
    settlement.recorded_tick = applied_at.half_day_tick;
    settlement.applied_at = applied_at;
    settlement.segment = session.segment;
    settlement.settled_at = session.now;
    take(common.append_fact(
        {settlement.fact_id, "field_battle_segment", "military"}));
    const std::vector<StableId> army_ids(armies.begin(), armies.end());
    auto receipt = take(apply_original_military_roster_casualties(
        common, military, settlement.fact_id, settlement.recorded_tick,
        army_ids, finals, std::move(officers), captors, true));
    settlement.casualty_fact_ids.push_back(receipt.id);
    MilitaryBattleStep out;
    out.casualty_source_ids.push_back(receipt.id);
    out.actor_changes = std::move(receipt.actor_changes);
    if (terminal) {
      MilitaryBattleFact fact;
      fact.id = take(common.next_id("battle_result"));
      fact.tick = settlement.recorded_tick;
      fact.start_tick = session.created_tick;
      fact.round = session.segment + 1;
      fact.session_id = battle_id;
      fact.army_ids = army_ids;
      fact.unit_ids = std::move(units);
      fact.result = MilitaryBattleResult{
          session.result->winner_side, session.result->reason, {}};
      fact.source_id = settlement.fact_id;
      fact.contact_source_id = contact->contact_id;
      fact.winner_side = session.result->winner_side;
      fact.final = true;
      take(common.append_fact({fact.id, "military_battle", "military"}));
      settlement.battle_fact_id = fact.id;
      out.round_fact_ids.push_back(fact.id);
      out.completed_battle_ids.push_back(battle_id);
      military.battle_facts.push_back(std::move(fact));
    }
    session.settlements.push_back(std::move(settlement));
    take(validate_field_segment_application_sources(common, military, world,
                                                    actual_clock));
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
