#include "field_event_legacy_kinds.hpp"
// 保留未定案方案与定案结果的区别；存读不把缺任务、重复将领或泄露情报修成有效军议。
#include "field_council_validation.hpp"
#include "field_command_source.hpp"
#include "field_validation.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
Status bad(const StableId &id) {
  return common_failure("InvalidFieldCouncilState", id);
}
bool same_source(const std::optional<SourceRef> &a,
                 const std::optional<SourceRef> &b) {
  if (bool(a) != bool(b))
    return false;
  return !a || (a->domain == b->domain && a->kind == b->kind && a->id == b->id);
}
bool point_equal(PointI a, PointI b) { return a.x == b.x && a.y == b.y; }
} // namespace
// 有限任务值的唯一相等判定供原事实/回执双向核对，不读取当前编队。
bool same_field_battle_mission(const FieldMission &a, const FieldMission &b) {
  if (a.index() != b.index())
    return false;
  return std::visit(
      [&](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        const auto &w = std::get<T>(b);
        if constexpr (std::is_same_v<T, FieldMoveTo> ||
                      std::is_same_v<T, FieldHold>)
          return point_equal(v.point_dm, w.point_dm) &&
                 v.facing_mdeg == w.facing_mdeg;
        else if constexpr (std::is_same_v<T, FieldEngage>)
          return v.target_formation_id == w.target_formation_id;
        else if constexpr (std::is_same_v<T, FieldSupport>)
          return v.ally_formation_id == w.ally_formation_id;
        else
          return v.exit_side == w.exit_side;
      },
      a);
}
namespace {
// 同一军议定案快照只允许之后补入侦察三级的敌方定案；其余来源不得冷读漂移。
template <class T, class Equal>
bool same_optional_rows(const std::optional<std::vector<T>> &a,
                        const std::optional<std::vector<T>> &b, Equal equal) {
  if (bool(a) != bool(b))
    return false;
  if (!a)
    return true;
  return a->size() == b->size() &&
         std::equal(a->begin(), a->end(), b->begin(), equal);
}
bool same_intel_source(const FieldCouncilIntelSource &a,
                       const FieldCouncilIntelSource &b) {
  return a.level == b.level && a.total_estimate == b.total_estimate &&
         same_optional_rows(a.formations, b.formations,
                            [](const auto &x, const auto &y) {
                              return x.formation_id == y.formation_id &&
                                     x.category == y.category && x.fit == y.fit;
                            }) &&
         same_optional_rows(
             a.commanders, b.commanders, [](const auto &x, const auto &y) {
               return x.actor_id == y.actor_id &&
                      x.leadership == y.leadership && x.martial == y.martial &&
                      x.intellect == y.intellect &&
                      x.administration == y.administration &&
                      x.charisma == y.charisma;
             });
}
bool same_request(const FieldCouncilRequest &a, const FieldCouncilRequest &b) {
  if (!same_source(a.command_result, b.command_result))
    return false;
  if (a.actor_id != b.actor_id || a.status != b.status ||
      a.reason != b.reason || a.value.index() != b.value.index())
    return false;
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto &y = std::get<T>(b.value);
        if constexpr (std::is_same_v<T, FieldCouncilRoleRequest>)
          return x.role == y.role;
        else
          return x.target_formation_id == y.target_formation_id;
      },
      a.value);
}
// 未定案建议可等主将补其本部安排；已定案则严格覆盖每一实际本侧编队。
Status plan(const FieldCouncilPlan &p, const FieldBattleSession &session,
            const GeneratedBattleField &field, Integer side, bool complete) {
  if (static_cast<Integer>(p.layout) < 0 || static_cast<Integer>(p.layout) > 3)
    return bad(session.battle_id);
  std::set<StableId> seen;
  for (const auto &a : p.assignments) {
    const auto f = session.formations.find(a.formation_id);
    if (f == session.formations.end() || f->second.side != side ||
        !seen.insert(a.formation_id).second ||
        static_cast<Integer>(a.wave) < 0 || static_cast<Integer>(a.wave) > 2)
      return bad(a.formation_id);
    if (a.flank_followup && (a.wave == FieldCouncilWave::Reserve ||
                             !std::holds_alternative<FieldMoveTo>(a.mission)))
      return bad(a.formation_id);
    auto valid =
        validate_field_battle_mission(a.mission, f->second, session, field);
    if (!valid.ok)
      return valid;
    if (a.wave == FieldCouncilWave::Reserve &&
        !std::holds_alternative<FieldHold>(a.mission))
      return bad(a.formation_id);
  }
  if (complete)
    for (const auto &[id, f] : session.formations)
      if (f.side == side && !seen.contains(id))
        return bad(id + "/missing_assignment");
  std::set<StableId> ambush;
  bool feint = false;
  for (const auto &s : p.stratagems) {
    if ((!complete && s.voided_at_adoption) ||
        (complete && s.voided_at_adoption ==
                         field_council_stratagem_assignment_eligible(s, p)))
      return bad(session.battle_id + "/stratagem_adoption");
    if (s.kind == FieldCouncilStratagemKind::Ambush) {
      if (!s.formation_id || !s.area_dm ||
          !ambush.insert(*s.formation_id).second)
        return bad(session.battle_id + "/ambush");
      const auto f = session.formations.find(*s.formation_id);
      if (f == session.formations.end() || f->second.side != side)
        return bad(*s.formation_id);
    } else if (s.kind == FieldCouncilStratagemKind::FeintStrength) {
      if (feint || s.formation_id || s.area_dm)
        return bad(session.battle_id + "/feint");
      feint = true;
    } else
      return bad(session.battle_id + "/stratagem");
  }
  return common_success();
}
} // namespace
bool same_field_council_plan(const FieldCouncilPlan &a,
                             const FieldCouncilPlan &b) {
  if (a.layout != b.layout || a.assignments.size() != b.assignments.size() ||
      a.stratagems.size() != b.stratagems.size())
    return false;
  for (std::size_t i = 0; i < a.assignments.size(); ++i) {
    const auto &x = a.assignments[i], &y = b.assignments[i];
    if (x.formation_id != y.formation_id || x.wave != y.wave ||
        x.flank_followup != y.flank_followup ||
        !same_field_battle_mission(x.mission, y.mission))
      return false;
  }
  for (std::size_t i = 0; i < a.stratagems.size(); ++i) {
    const auto &x = a.stratagems[i], &y = b.stratagems[i];
    if (x.kind != y.kind || x.formation_id != y.formation_id ||
        x.voided_at_adoption != y.voided_at_adoption ||
        !same_source(x.command_result, y.command_result) ||
        bool(x.area_dm) != bool(y.area_dm) ||
        (x.area_dm && !point_equal(*x.area_dm, *y.area_dm)))
      return false;
  }
  return true;
}
Status validate_field_council_records(const CommonStore &common,
                                      const FieldBattleSession &session,
                                      const GeneratedBattleField &field) {
  for (Integer side = 0; side < 2; ++side) {
    const auto &c = session.councils[side];
    std::set<StableId> actors, formations, proposals;
    Integer commanders = 0, strategists = 0;
    std::optional<StableId> strategist;
    for (const auto &p : c.participants) {
      const auto f = session.formations.find(p.formation_id);
      if (!common.actor(p.actor_id) || f == session.formations.end() ||
          f->second.side != side ||
          f->second.commander_actor_id != p.actor_id ||
          !actors.insert(p.actor_id).second ||
          !formations.insert(p.formation_id).second ||
          static_cast<Integer>(p.stance) < 0 ||
          static_cast<Integer>(p.stance) > 4)
        return bad(p.actor_id);
      if (p.role == FieldCouncilRole::Commander) {
        if (p.actor_id != session.sides[side].commander_actor_id)
          return bad(p.actor_id);
        ++commanders;
      } else if (p.role == FieldCouncilRole::Strategist) {
        if (p.actor_id == session.sides[side].commander_actor_id)
          return bad(p.actor_id);
        ++strategists;
        strategist = p.actor_id;
      } else if (p.role != FieldCouncilRole::Officer)
        return bad(p.actor_id);
    }
    if (c.player_actor_id &&
        (!actors.contains(*c.player_actor_id) || c.player_actor_id->empty()))
      return bad(session.battle_id + "/council_player");
    if (!c.closed && !c.player_actor_id)
      return bad(session.battle_id + "/council_wait_owner");
    if (commanders != 1 || strategists > 1)
      return bad(session.battle_id + "/council_roles");
    for (const auto &[id, f] : session.formations)
      if (f.side == side && !formations.contains(id))
        return bad(id + "/council_participant");
    for (const auto &p : c.proposals) {
      if (p.proposal_id.empty() || p.text_key.empty() ||
          !actors.contains(p.speaker_actor_id) ||
          !proposals.insert(p.proposal_id).second)
        return bad(p.proposal_id);
      auto participant = std::find_if(
          c.participants.begin(), c.participants.end(),
          [&](const auto &v) { return v.actor_id == p.speaker_actor_id; });
      if (participant == c.participants.end() ||
          participant->stance != p.stance)
        return bad(p.proposal_id);
      auto valid = plan(p.plan, session, field, side,
                        c.closed && c.adopted_proposal_id &&
                            *c.adopted_proposal_id == p.proposal_id);
      if (!valid.ok)
        return valid;
    }
    if (c.closed != bool(c.adopted_proposal_id) ||
        (c.adopted_proposal_id && !proposals.contains(*c.adopted_proposal_id)))
      return bad(session.battle_id + "/adoption");
    if (c.requests.size() > 1)
      return bad(session.battle_id + "/council_spoken_twice");
    for (const auto &r : c.requests) {
      if (!c.player_actor_id || r.actor_id != *c.player_actor_id ||
          r.actor_id == session.sides[side].commander_actor_id ||
          !actors.contains(r.actor_id) || static_cast<Integer>(r.status) < 1 ||
          static_cast<Integer>(r.status) > 2 || r.reason.empty())
        return bad(r.actor_id + "/request");
      if (const auto *role = std::get_if<FieldCouncilRoleRequest>(&r.value)) {
        if (static_cast<Integer>(role->role) < 0 ||
            static_cast<Integer>(role->role) > 3)
          return bad(r.actor_id);
      } else {
        const auto target = session.formations.find(
            std::get<FieldCouncilTargetRequest>(r.value).target_formation_id);
        if (target == session.formations.end() || target->second.side == side)
          return bad(r.actor_id);
      }
    }
    const auto &intel = c.intel;
    if (intel.level < 0 || intel.level > 3 || intel.total_estimate[0] < 0 ||
        intel.total_estimate[1] < intel.total_estimate[0] ||
        bool(intel.formations) != (intel.level >= 1) ||
        bool(intel.commanders) != (intel.level >= 2))
      return bad(session.battle_id + "/intel");
    std::set<StableId> reported;
    if (intel.formations)
      for (const auto &r : *intel.formations) {
        const auto f = session.formations.find(r.formation_id);
        if (f == session.formations.end() || f->second.side == side ||
            f->second.category != r.category || f->second.fit_start != r.fit ||
            !reported.insert(r.formation_id).second)
          return bad(r.formation_id + "/intel");
      }
    if (intel.formations)
      for (const auto &[id, f] : session.formations)
        if (f.side != side && !reported.contains(id))
          return bad(id + "/intel_missing");
    reported.clear();
    if (intel.commanders)
      for (const auto &r : *intel.commanders) {
        bool belongs = false;
        for (const auto &[id, f] : session.formations)
          belongs |= f.side != side && f.commander_actor_id == r.actor_id;
        if (!belongs || !reported.insert(r.actor_id).second)
          return bad(r.actor_id + "/intel");
        for (auto attribute : {r.leadership, r.martial, r.intellect,
                               r.administration, r.charisma})
          if (attribute < 1 || attribute > 110)
            return bad(r.actor_id + "/intel_attribute");
      }
    const auto &enemy = session.councils[1 - side];
    if (bool(intel.enemy_plan) != (intel.level == 3 && enemy.closed))
      return bad(session.battle_id + "/intel_plan");
    if (intel.enemy_plan) {
      auto p = std::find_if(
          enemy.proposals.begin(), enemy.proposals.end(), [&](const auto &v) {
            return enemy.adopted_proposal_id &&
                   v.proposal_id == *enemy.adopted_proposal_id;
          });
      if (p == enemy.proposals.end() ||
          !same_field_council_plan(*intel.enemy_plan, p->plan))
        return bad(session.battle_id + "/intel_plan");
    }
  }
  if (session.councils[0].player_actor_id &&
      session.councils[1].player_actor_id)
    return bad(session.battle_id + "/council_two_players");
  const bool closed = session.councils[0].closed && session.councils[1].closed;
  if ((session.state == FieldBattleState::Council) == closed)
    return bad(session.battle_id + "/phase");
  if ((session.state == FieldBattleState::Running ||
       session.state == FieldBattleState::Ended) != bool(session.started_at))
    return bad(session.battle_id + "/started_at");
  if (session.started_at &&
      (session.started_at->tick < session.created_tick - 1 ||
       session.started_at->t < 0 ||
       session.started_at->t >= field.parameters.window_s ||
       session.started_at->t % field.parameters.step_s ||
       std::tie(session.started_at->tick, session.started_at->t) >
           std::tie(session.now.tick, session.now.t)))
    return bad(session.battle_id + "/started_at");
  return common_success();
}
Status
validate_field_council_facts(const CommonStore &common,
                             const FieldBattleWorldState &world,
                             const FamilyLifeHistoryRead4 *original_life) {
  std::set<std::pair<StableId, Integer>> adopted;
  std::set<std::pair<StableId, StableId>> delivered;
  std::set<std::tuple<StableId, StableId, Integer>> ai_revisions;
  std::set<StableId> ai_orders, ai_activations;
  for (const auto &[id, fact] : world.council_facts) {
    const auto *head = common.state().facts.find(id);
    const auto s = world.sessions.find(fact.battle_id);
    if (id != fact.fact_id || !head || head->kind != "military_council" ||
        head->domain != "military" || s == world.sessions.end() ||
        fact.side < 0 || fact.side > 1 || fact.recorded_tick < 0 ||
        fact.recorded_tick > common.tick() ||
        fact.recorded_tick < s->second.created_tick ||
        std::tie(fact.at.tick, fact.at.t) >
            std::tie(s->second.now.tick, s->second.now.t))
      return bad(id);
    if (bool(fact.adopted_actual_at) !=
        (fact.kind == FieldCouncilFactKind::Adopted))
      return bad(id);
    if (fact.adopted_actual_at) {
      if (fact.adopted_actual_at->half_day_tick != fact.recorded_tick)
        return bad(id);
      auto moment = compare_world_moments(*fact.adopted_actual_at,
                                          *fact.adopted_actual_at);
      if (!moment.ok)
        return {false, {}, moment.error};
    }
    if (fact.kind == FieldCouncilFactKind::AiOrder) {
      auto original =
          read_field_ai_command_source(common, world, id, original_life);
      if (!original.ok)
        return {false, {}, original.error};
      if (fact.mission_revision &&
          !ai_revisions
               .emplace(fact.battle_id, *fact.formation_id,
                        *fact.mission_revision)
               .second)
        return bad(id);
      if (fact.order_id && !ai_orders.insert(*fact.order_id).second)
        return bad(id);
      if (fact.activation_id &&
          !ai_activations.insert(*fact.activation_id).second)
        return bad(id);
      continue;
    }
    if (fact.ai_pursuit || fact.ai_control_acceptance)
      return bad(id);
    if (fact.kind == FieldCouncilFactKind::OrderDelivered) {
      const auto &session = s->second;
      if (!fact.order_id || !fact.player_actor_id || !fact.formation_id ||
          fact.command_result || fact.player_command_kind || fact.tactic_id ||
          fact.activation_id || fact.player_mission || fact.mission_revision ||
          fact.council.player_actor_id || !fact.council.participants.empty() ||
          !fact.council.proposals.empty() || !fact.council.requests.empty() ||
          fact.council.closed || fact.council.adopted_proposal_id ||
          fact.council.intel.level != 0 ||
          fact.council.intel.total_estimate != std::array<Integer, 2>{0, 0} ||
          fact.council.intel.formations || fact.council.intel.commanders ||
          fact.council.intel.enemy_plan ||
          session.sides[fact.side].commander_actor_id ==
              *fact.player_actor_id ||
          !delivered.insert({fact.battle_id, *fact.order_id}).second)
        return bad(id);
      const auto order = session.orders.find(*fact.order_id);
      const auto formation = session.formations.find(*fact.formation_id);
      if (order == session.orders.end() ||
          formation == session.formations.end() ||
          formation->second.side != fact.side ||
          order->second.target_formation_id != *fact.formation_id ||
          order->second.status != FieldOrderStatus::Delivered ||
          !order->second.status_at ||
          order->second.status_at->tick != fact.at.tick ||
          order->second.status_at->t != fact.at.t)
        return bad(id);
      auto original_control =
          read_field_original_control_before_order_resolution(
              common, world, session, order->second);
      if (!original_control.ok)
        return {false, {}, original_control.error};
      if (!original_control.value.direct ||
          original_control.value.controller_actor_id != *fact.player_actor_id)
        return bad(id);
      continue;
    }
    if (fact.kind == FieldCouncilFactKind::PlayerOrder) {
      const auto &session = s->second;
      if (!fact.player_command_kind || !fact.player_actor_id ||
          !fact.formation_id || !fact.command_result ||
          session.councils[fact.side].player_actor_id != fact.player_actor_id ||
          session.sides[fact.side].commander_actor_id ==
              *fact.player_actor_id ||
          !session.formations.contains(*fact.formation_id) ||
          session.formations.at(*fact.formation_id).side != fact.side ||
          fact.council.player_actor_id || !fact.council.participants.empty() ||
          !fact.council.proposals.empty() || !fact.council.requests.empty() ||
          fact.council.closed || fact.council.adopted_proposal_id ||
          fact.council.intel.level != 0 ||
          fact.council.intel.total_estimate != std::array<Integer, 2>{0, 0} ||
          fact.council.intel.formations || fact.council.intel.commanders ||
          fact.council.intel.enemy_plan)
        return bad(id);
      const auto &kind = *fact.player_command_kind;
      const bool direct = kind == "field_battle_direct";
      const bool order = kind == "field_battle_order";
      const bool dispatch = kind == "field_battle_dispatch";
      const bool rejoin = kind == "field_battle_rejoin";
      const bool tactic = kind == "field_battle_tactic";
      if ((!direct && !order && !dispatch && !rejoin && !tactic) ||
          bool(fact.player_mission) != (direct || order || dispatch) ||
          bool(fact.mission_revision) != (direct || dispatch) ||
          (!tactic && bool(fact.order_id) != order) ||
          bool(fact.tactic_id) != tactic || (!tactic && fact.activation_id) ||
          (tactic && bool(fact.order_id) == bool(fact.activation_id)) ||
          (fact.mission_revision && *fact.mission_revision <= 0))
        return bad(id);
      if (tactic) {
        if (fact.order_id) {
          const auto found = session.orders.find(*fact.order_id);
          if (found == session.orders.end() ||
              found->second.issuer_actor_id != *fact.player_actor_id ||
              found->second.target_formation_id != *fact.formation_id ||
              !std::holds_alternative<FieldTacticIntent>(
                  found->second.payload) ||
              std::get<FieldTacticIntent>(found->second.payload).tactic_id !=
                  *fact.tactic_id)
            return bad(id);
        } else {
          const auto &formation = session.formations.at(*fact.formation_id);
          bool found = false;
          if (formation.ability)
            for (const auto &activation : formation.ability->active)
              if (activation.activation_id == *fact.activation_id &&
                  activation.tactic_id == *fact.tactic_id &&
                  activation.issuer_actor_id == *fact.player_actor_id &&
                  activation.started.tick == fact.at.tick &&
                  activation.started.t == fact.at.t)
                found = true;
          if (!found)
            return bad(id);
        }
      }
      if (order) {
        const auto found = session.orders.find(*fact.order_id);
        if (found == session.orders.end() ||
            found->second.issuer_actor_id != *fact.player_actor_id ||
            found->second.target_formation_id != *fact.formation_id ||
            !std::holds_alternative<FieldMission>(found->second.payload) ||
            !same_field_battle_mission(
                *fact.player_mission,
                std::get<FieldMission>(found->second.payload)))
          return bad(id);
      }
      continue;
    }
    if (fact.player_command_kind || fact.player_actor_id || fact.formation_id ||
        fact.player_mission || fact.mission_revision || fact.tactic_id ||
        fact.activation_id || fact.recorded_tick != s->second.created_tick ||
        fact.kind != FieldCouncilFactKind::Adopted || fact.order_id ||
        !fact.council.closed || !fact.council.adopted_proposal_id ||
        !adopted.insert({fact.battle_id, fact.side}).second ||
        fact.at.tick != s->second.started_at.value_or(s->second.now).tick ||
        fact.at.t != s->second.started_at.value_or(s->second.now).t ||
        std::tie(fact.at.tick, fact.at.t) >
            std::tie(s->second.now.tick, s->second.now.t))
      return bad(id);
    const auto &current = s->second.councils[fact.side];
    if (current.player_actor_id != fact.council.player_actor_id ||
        current.adopted_proposal_id != fact.council.adopted_proposal_id ||
        current.participants.size() != fact.council.participants.size() ||
        current.proposals.size() != fact.council.proposals.size() ||
        current.requests.size() != fact.council.requests.size() ||
        !std::equal(current.requests.begin(), current.requests.end(),
                    fact.council.requests.begin(), same_request) ||
        !same_intel_source(current.intel, fact.council.intel) ||
        (fact.council.intel.enemy_plan &&
         (!current.intel.enemy_plan ||
          !same_field_council_plan(*current.intel.enemy_plan,
                                   *fact.council.intel.enemy_plan))))
      return bad(id);
    for (std::size_t i = 0; i < current.participants.size(); ++i) {
      const auto &a = current.participants[i],
                 &b = fact.council.participants[i];
      if (a.actor_id != b.actor_id || a.formation_id != b.formation_id ||
          a.role != b.role || a.stance != b.stance)
        return bad(id);
    }
    for (std::size_t i = 0; i < current.proposals.size(); ++i) {
      const auto &a = current.proposals[i], &b = fact.council.proposals[i];
      if (a.proposal_id != b.proposal_id ||
          a.speaker_actor_id != b.speaker_actor_id || a.stance != b.stance ||
          a.text_key != b.text_key ||
          !same_source(a.command_result, b.command_result) ||
          !same_field_council_plan(a.plan, b.plan))
        return bad(id);
    }
  }
  for (const auto &[id, s] : world.sessions)
    for (Integer side = 0; side < 2; ++side)
      if (s.councils[side].closed && !adopted.contains({id, side}))
        return bad(id + "/council_fact");
  for (const auto &[battle_id, session] : world.sessions)
    for (const auto &[order_id, order] : session.orders)
      if (order.status == FieldOrderStatus::Delivered) {
        const auto formation =
            session.formations.find(order.target_formation_id);
        if (formation == session.formations.end())
          return bad(order_id);
        const auto side = formation->second.side;
        auto control = read_field_original_control_before_order_resolution(
            common, world, session, order);
        if (!control.ok)
          return {false, {}, control.error};
        if (!control.value.direct)
          return bad(order_id);
        if (session.sides[side].commander_actor_id !=
                control.value.controller_actor_id &&
            !delivered.contains({battle_id, order_id}))
          return bad(order_id + "/delivery_fact_missing");
      }
  for (const auto &[battle_id, session] : world.sessions) {
    std::set<StableId> fact_events, request_events;
    for (const auto &event : session.events) {
      if (event.kind != field_event_kind::CouncilAdopted &&
          event.kind != field_event_kind::RequestGranted &&
          event.kind != field_event_kind::RequestDenied)
        continue;
      if (!event.side || *event.side < 0 || *event.side > 1 || !event.source_id)
        return bad(battle_id + "/council_event");
      if (event.kind == field_event_kind::CouncilAdopted) {
        const auto fact = world.council_facts.find(*event.source_id);
        if (fact == world.council_facts.end() || event.formation_id ||
            fact->second.battle_id != battle_id ||
            fact->second.side != *event.side ||
            fact->second.at.tick != event.at.tick ||
            fact->second.at.t != event.at.t ||
            !fact_events.insert(*event.source_id).second)
          return bad(battle_id + "/adopt_event");
      } else if (event.kind == field_event_kind::RequestGranted ||
                 event.kind == field_event_kind::RequestDenied) {
        const auto &council = session.councils[*event.side];
        if (council.requests.size() != 1 || !event.formation_id)
          return bad(battle_id + "/request_event");
        const auto &request = council.requests.front();
        const auto formation = session.formations.find(*event.formation_id);
        if (formation == session.formations.end() ||
            formation->second.side != *event.side ||
            formation->second.commander_actor_id != request.actor_id ||
            !request.command_result ||
            request.command_result->id != *event.source_id ||
            (event.kind == field_event_kind::RequestGranted) !=
                (request.status == FieldCouncilRequestStatus::Granted) ||
            !request_events.insert(request.actor_id).second)
          return bad(battle_id + "/request_event");
      }
    }
    for (const auto &[id, fact] : world.council_facts)
      if (fact.battle_id == battle_id &&
          fact.kind == FieldCouncilFactKind::Adopted &&
          !fact_events.contains(id))
        return bad(id + "/adopt_event_missing");
    for (const auto &council : session.councils)
      for (const auto &request : council.requests)
        if (!request_events.contains(request.actor_id))
          return bad(request.actor_id + "/request_event_missing");
  }
  for (const auto &[id, h] : common.state().facts.rows())
    if (h.kind == "military_council" &&
        (h.domain != "military" || !world.council_facts.contains(id)))
      return bad(id);
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
bool field_council_stratagem_assignment_eligible(
    const FieldCouncilStratagemPlan &item, const FieldCouncilPlan &chosen) {
  if (item.kind == FieldCouncilStratagemKind::FeintStrength)
    return !item.formation_id && !item.area_dm;
  if (item.kind != FieldCouncilStratagemKind::Ambush || !item.formation_id ||
      !item.area_dm)
    return false;
  const FieldCouncilAssignment *assigned = nullptr;
  for (const auto &a : chosen.assignments)
    if (a.formation_id == *item.formation_id) {
      if (assigned)
        return false;
      assigned = &a;
    }
  if (!assigned)
    return false;
  const auto *hold = std::get_if<FieldHold>(&assigned->mission);
  return hold && hold->point_dm.x == item.area_dm->x &&
         hold->point_dm.y == item.area_dm->y;
}
} // namespace hanmo::resident
