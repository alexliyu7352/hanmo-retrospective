#include "resident/model/common_model.hpp"
#include "field_ai_decision.hpp"
// 本 TU 读取原会话内部字段，直接依赖完整定义，不回灌公开视图头。
#include "field_session.hpp"
#include "field_integer_geometry.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool present(const FieldFormationView &f) {
  return f.fit > 0 && f.state != "Withdrawn" && f.state != "Destroyed" &&
         f.state != "Deploying";
}
bool confused(const FieldFormationView &f, const FieldBattleTime &now) {
  return std::any_of(f.statuses.begin(), f.statuses.end(), [&](const auto &s) {
    return s.kind == "confused" &&
           (!s.until ||
            std::tie(now.tick, now.t) < std::tie(s.until->tick, s.until->t));
  });
}
__int128_t seconds(const FieldBattleTime &later, const FieldBattleTime &earlier,
                   Integer window) {
  return (__int128_t(later.tick) - earlier.tick) * window + later.t - earlier.t;
}
} // namespace
Outcome<FieldAiDecisionRead> read_field_ai_mission_decisions(
    const FieldBattleSession &session, const FieldBattleView &view,
    const GeneratedBattleField &field, Integer side) {
  auto fail = [](const char *code) -> Outcome<FieldAiDecisionRead> {
    return {false, {}, common_failure(code).error};
  };
  if (side < 0 || side > 1 || session.state != FieldBattleState::Running ||
      view.identity.state != session.state ||
      view.identity.battle_id != session.battle_id ||
      view.identity.now.tick != session.now.tick ||
      view.identity.now.t != session.now.t ||
      view.field.field_hash != session.field_hash ||
      field.field_hash != session.field_hash ||
      view.identity.observer_actor_id !=
          session.sides[side].commander_actor_id ||
      !std::holds_alternative<FieldAiController>(
          session.sides[side].controller))
    return fail("FieldAiViewSourceMismatch");
  const auto &p = field.parameters;
  auto valid = validate_field_battle_parameters(p);
  if (!valid.ok)
    return {false, {}, valid.error};
  FieldAiDecisionRead out;
  if (session.ai[side].pending_triggers.empty() &&
      seconds(session.now, session.ai[side].last_eval, p.window_s) <
          p.ai_eval_s)
    return {true, std::move(out), {}};
  out.evaluated = true;
  std::map<StableId, const FieldFormationView *> own, enemy;
  std::set<StableId> ids;
  for (const auto &f : view.formations) {
    if (!ids.insert(f.formation_id).second || f.side < 0 || f.side > 1)
      return fail("FieldAiViewFormationRepeated");
    if (f.side == side)
      own.emplace(f.formation_id, &f);
    else if (present(f))
      enemy.emplace(f.formation_id, &f);
  }
  __int128_t start = 0, fit = 0, morale = 0, count = 0;
  for (const auto &[id, original] : session.formations) {
    if (original.side != side)
      continue;
    start += original.fit_start;
    if (original.state == FieldFormationState::Withdrawn ||
        original.state == FieldFormationState::Destroyed ||
        original.fit.empty())
      continue;
    const auto found = own.find(id);
    if (found == own.end() ||
        found->second->fit != Integer(original.fit.size()) ||
        found->second->morale != original.morale)
      return fail("FieldAiOwnViewMismatch");
    fit += found->second->fit;
    morale += found->second->morale;
    ++count;
  }
  std::map<StableId, FieldAiMissionDecision> chosen;
  auto available = [&](const FieldFormationView &f) {
    return present(f) && f.state != "Routing" && f.state != "Engaged" &&
           !confused(f, session.now) && !chosen.contains(f.formation_id);
  };
  auto nearest = [&](const PointI &point,
                     auto predicate) -> const FieldFormationView * {
    const FieldFormationView *best = nullptr;
    __int128_t distance = 0;
    for (const auto &[id, f] : own) {
      if (!available(*f) || !predicate(*f))
        continue;
      const auto next = field_distance_squared(point, f->position_dm);
      if (!best || next < distance) {
        best = f;
        distance = next;
      }
    }
    return best;
  };
  const Integer threshold =
      session.sides[side].feint_strength_id ? 50 : p.ai_retreat_fit_percent;
  if (start > 0 && (fit * 100 < start * threshold ||
                    (count > 0 && morale < count * p.ai_retreat_morale))) {
    for (const auto &[id, f] : own)
      if (present(*f) && !confused(*f, session.now))
        chosen.emplace(id,
                       FieldAiMissionDecision{id, FieldWithdraw{side}, false});
  } else {
    // 同距离由原 map 的 formation_id 次序决定；不触碰敌方 orders。
    for (const auto &[id, target] : enemy) {
      if (target->state != "Routing")
        continue;
      auto selected = nearest(target->position_dm, [](const auto &f) {
        return f.category == "Cavalry";
      });
      if (!selected)
        selected = nearest(target->position_dm, [](const auto &f) {
          return f.wave == FieldCouncilWave::Reserve;
        });
      if (selected)
        chosen.emplace(selected->formation_id,
                       FieldAiMissionDecision{selected->formation_id,
                                              FieldEngage{id}, true});
    }
    for (const auto &[id, target] : own) {
      if (target->state != "Engaged" || target->morale >= p.ai_support_morale)
        continue;
      auto selected = nearest(target->position_dm, [&](const auto &f) {
        return f.category == "Infantry" && f.formation_id != id;
      });
      if (selected)
        chosen.emplace(selected->formation_id,
                       FieldAiMissionDecision{selected->formation_id,
                                              FieldSupport{id}, false});
    }
    const FieldCouncilPlan *plan = nullptr;
    const auto &council = session.councils[side];
    for (const auto &proposal : council.proposals)
      if (council.adopted_proposal_id == proposal.proposal_id) {
        if (plan)
          return fail("FieldAiCouncilSourceRepeated");
        plan = &proposal.plan;
      }
    if (!plan)
      return fail("FieldAiCouncilSourceRequired");
    if (plan->layout == FieldCouncilLayout::Line ||
        plan->layout == FieldCouncilLayout::Flank) {
      for (const auto &[id, f] : own) {
        const auto original = session.formations.find(id);
        if (!available(*f) || f->state != "Holding" ||
            f->wave != FieldCouncilWave::First ||
            original == session.formations.end() ||
            !original->second.hold_since ||
            seconds(session.now, *original->second.hold_since, p.window_s) <=
                p.ai_advance_hold_s)
          continue;
        const FieldFormationView *target = nullptr;
        __int128_t distance = 0;
        for (const auto &[other_id, other] : enemy) {
          const auto next =
              field_distance_squared(f->position_dm, other->position_dm);
          if (!target || next < distance) {
            target = other;
            distance = next;
          }
        }
        if (target)
          chosen.emplace(id, FieldAiMissionDecision{
                                 id, FieldEngage{target->formation_id}, false});
      }
    }
  }
  for (auto &[id, decision] : chosen)
    out.missions.push_back(std::move(decision));
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
