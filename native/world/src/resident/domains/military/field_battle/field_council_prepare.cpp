#include "field_council_validation.hpp"
// 军议仅从真实将领和地形构造计划；不是表现层假会战移植。
#include "field_council_prepare.hpp"
#include "field_flank.hpp"
#include "field_integer_geometry.hpp"
#include "field_validation.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
using Wide = __int128_t;
// 未能证明来源/地形的候选不变成默认 Hold 或虚构一个敌编队。
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {},
                ErrorKind kind = ErrorKind::rule) {
  return {false, {}, common_failure(code, id, kind).error};
}
// 原会话侧区必须与实际生成侧区相配，反向追击不可直接按数组下标取作者端。
const FieldSideZone *zone(const FieldBattleSession &session,
                          const GeneratedBattleField &field, Integer side) {
  auto original = read_field_side_geometry(session, field, side);
  return original.ok ? original.value : nullptr;
}
// 并列依 §6.3 原 formation_id；只在真实本场名册内选目标。
const FieldFormation *nearest(const FieldBattleSession &session, Integer side,
                              FieldTroopCategory category, PointI point,
                              const FieldDeploymentLayout *layout = nullptr) {
  const FieldFormation *best = nullptr;
  Wide distance = 0;
  for (const auto &[id, f] : session.formations) {
    if (f.side != side || f.category != category || f.fit.empty())
      continue;
    PointI at = f.position_dm;
    if (layout) {
      const auto p =
          std::find_if(layout->placements.begin(), layout->placements.end(),
                       [&](const auto &p) { return p.formation_id == id; });
      if (p == layout->placements.end())
        continue;
      at = p->point_dm;
    }
    const auto d = field_distance_squared(point, at);
    if (!best || d < distance || (d == distance && id < best->formation_id)) {
      best = &f;
      distance = d;
    }
  }
  return best;
}
// 林/崎岖必须距本侧实际布阵区不超过 600 米；选择距离最近的合法格，等距按格序。
std::optional<PointI> ambush_cell(const FieldBattleSession &session,
                                  const GeneratedBattleField &field,
                                  Integer side) {
  std::optional<std::size_t> best;
  Wide best_distance = 0;
  for (std::size_t i = 0; i < field.cells.size(); ++i) {
    if (field.cells[i].terrain != FieldTerrainKind::Forest &&
        field.cells[i].terrain != FieldTerrainKind::Rough)
      continue;
    const auto point = field_cell_center_dm(field, i);
    std::optional<Wide> closest;
    for (auto c : session.sides[side].deploy_zone) {
      if (c >= field.cells.size())
        return {};
      const auto d =
          field_distance_squared(point, field_cell_center_dm(field, c));
      if (!closest || d < *closest)
        closest = d;
    }
    if (closest && *closest <= Wide(6000) * 6000 &&
        (!best || *closest < best_distance ||
         (*closest == best_distance && i < *best))) {
      best = i;
      best_distance = *closest;
    }
  }
  return best ? std::optional<PointI>{field_cell_center_dm(field, *best)}
              : std::nullopt;
}
// 沿本侧摆位面向敌出口推进合同的 800 米；整数方向只在最终分米量化。
Outcome<PointI> forward(PointI from, PointI to, Integer distance_dm) {
  const auto norm = field_integer_sqrt(field_distance_squared(from, to));
  if (norm <= 0)
    return fail<PointI>("InvalidFieldBattleRoute");
  return {true,
          {from.x + Integer(Wide(to.x - from.x) * distance_dm / norm),
           from.y + Integer(Wide(to.y - from.y) * distance_dm / norm)},
          {}};
}
// 计划先使用原布局几何；按用户已定规则，中军 Hold 只让主将本人的先锋方案例外。
Outcome<FieldCouncilPlan> plan(const FieldBattleSession &session,
                               const GeneratedBattleField &field, Integer side,
                               const FieldCouncilOfficerSource &speaker) {
  const auto *own = zone(session, field, side),
             *enemy = zone(session, field, 1 - side);
  if (!own || !enemy)
    return fail<FieldCouncilPlan>("InvalidFieldBattleRoute");
  const auto concealed = speaker.stance == FieldCouncilStance::Cunning
                             ? ambush_cell(session, field, side)
                             : std::nullopt;
  const auto strategy = speaker.stance == FieldCouncilStance::Cautious
                            ? FieldAiStrategy::HoldGround
                        : speaker.stance == FieldCouncilStance::Cavalry
                            ? FieldAiStrategy::Flanking
                            : FieldAiStrategy::LineReserve;
  auto layout = prepare_field_deployment_layout(session, field, side, strategy);
  if (!layout.ok)
    return {false, {}, layout.error};
  FieldCouncilPlan out;
  out.layout =
      strategy == FieldAiStrategy::HoldGround ? FieldCouncilLayout::Hold
      : strategy == FieldAiStrategy::Flanking ? FieldCouncilLayout::Flank
      : concealed                             ? FieldCouncilLayout::Ambush
                                              : FieldCouncilLayout::Line;
  for (const auto &p : layout.value.placements) {
    const auto it = session.formations.find(p.formation_id);
    if (it == session.formations.end())
      return fail<FieldCouncilPlan>("FieldCouncilSourceRequired",
                                    p.formation_id);
    const auto &f = it->second;
    FieldCouncilAssignment a;
    a.formation_id = p.formation_id;
    a.wave = p.reserve ? FieldCouncilWave::Reserve : FieldCouncilWave::First;
    const bool commander_vanguard =
        p.commander &&
        speaker.actor_id == session.sides[side].commander_actor_id &&
        speaker.stance == FieldCouncilStance::Aggressive;
    if (p.commander && !commander_vanguard) {
      a.mission = FieldHold{p.point_dm, p.facing_mdeg};
    } else if (strategy == FieldAiStrategy::HoldGround) {
      a.mission = FieldHold{p.point_dm, p.facing_mdeg};
    } else if (speaker.stance == FieldCouncilStance::Aggressive) {
      // 中路目标取敌实际编队在敌布阵中心最近者；不创建占位敌人。
      const FieldFormation *target = nullptr;
      Wide distance = 0;
      for (const auto &[id, e] : session.formations)
        if (e.side != side && !e.fit.empty()) {
          // 建议时敌军未摆位，使用其真实类别布阵后位置，避免默认(0,0)冒位置。
          auto enemy_layout =
              prepare_field_deployment_layout(session, field, 1 - side);
          if (!enemy_layout.ok)
            return {false, {}, enemy_layout.error};
          auto place =
              std::find_if(enemy_layout.value.placements.begin(),
                           enemy_layout.value.placements.end(),
                           [&](const auto &x) { return x.formation_id == id; });
          if (place == enemy_layout.value.placements.end())
            return fail<FieldCouncilPlan>("FieldCouncilSourceRequired", id);
          auto d = field_distance_squared(place->point_dm,
                                          enemy->deployment_center_dm);
          if (!target || d < distance ||
              (d == distance && id < target->formation_id)) {
            target = &e;
            distance = d;
          }
        }
      if (!target)
        return fail<FieldCouncilPlan>("FieldCouncilSourceRequired",
                                      session.battle_id);
      a.mission = FieldEngage{target->formation_id};
      a.wave = f.formation_id == speaker.formation_id
                   ? FieldCouncilWave::First
                   : FieldCouncilWave::Second;
    } else if (concealed && f.formation_id == speaker.formation_id) {
      a.mission = FieldHold{*concealed, p.facing_mdeg};
      out.stratagems.push_back(
          {FieldCouncilStratagemKind::Ambush, f.formation_id, *concealed});
    } else if (p.reserve) {
      a.mission = FieldHold{p.point_dm, p.facing_mdeg};

    } else if (f.category == FieldTroopCategory::Archer) {
      const auto *ally = nearest(session, side, FieldTroopCategory::Infantry,
                                 p.point_dm, &layout.value);
      if (!ally)
        return fail<FieldCouncilPlan>("FieldCouncilSupportTargetRequired",
                                      f.formation_id);
      a.mission = FieldSupport{ally->formation_id};
    } else {
      auto target = forward(p.point_dm, enemy->road_exit_dm, 8000);
      if (!target.ok)
        return {false, {}, target.error};
      a.mission = FieldMoveTo{target.value, p.facing_mdeg};
      if (strategy == FieldAiStrategy::Flanking &&
          f.category == FieldTroopCategory::Cavalry) {
        auto flank = read_field_flank_target(session, field, side, p);
        if (!flank.ok)
          return {false, {}, flank.error};
        a.mission = flank.value;
        a.flank_followup = true;
      }
    }
    out.assignments.push_back(std::move(a));
  }
  if (speaker.stance == FieldCouncilStance::Cunning && !concealed)
    out.stratagems.push_back(
        {FieldCouncilStratagemKind::FeintStrength, {}, {}});
  return {true, std::move(out), {}};
}
} // namespace
Outcome<std::array<FieldCouncilRecord, 2>> prepare_field_councils(
    const CommonStore &common, const SocialState &social,
    const MilitaryState &military, const OrganizationState *organization,
    const CheckpointMap<StableId, StableId> &titles,
    const FieldBattleSession &session, const GeneratedBattleField &field,
    const StableId &player, const FieldBattleAbilityReadContext &abilities) {
  auto source =
      read_field_council_sources(common, session, field, social, military,
                                 titles, organization, abilities);
  if (!source.ok)
    return {false, {}, source.error};
  std::array<FieldCouncilRecord, 2> result;
  for (Integer side = 0; side < 2; ++side) {
    const auto &s = source.value.sides[side];
    auto &out = result[side];
    out.intel = source.value.intel[side];
    std::vector<const FieldCouncilOfficerSource *> speakers;
    for (const auto &officer : s.officers) {
      const auto role =
          officer.actor_id == s.commander_actor_id ? FieldCouncilRole::Commander
          : s.strategist_actor_id && officer.actor_id == *s.strategist_actor_id
              ? FieldCouncilRole::Strategist
              : FieldCouncilRole::Officer;
      out.participants.push_back(
          {officer.actor_id, role, officer.formation_id, officer.stance});
      if (officer.actor_id == player) {
        out.player_actor_id = player;
        continue;
      }
      auto same = std::find_if(speakers.begin(), speakers.end(), [&](auto *v) {
        return v->stance == officer.stance;
      });
      if (same == speakers.end())
        speakers.push_back(&officer);
      else if (officer.official_rank > (*same)->official_rank ||
               (officer.official_rank == (*same)->official_rank &&
                (officer.fit > (*same)->fit ||
                 (officer.fit == (*same)->fit &&
                  officer.formation_id < (*same)->formation_id))))
        *same = &officer;
    }
    for (const auto *speaker : speakers) {
      auto p = plan(session, field, side, *speaker);
      if (!p.ok)
        return {false, {}, p.error};
      FieldCouncilProposal proposal;
      proposal.proposal_id =
          session.battle_id + "/proposal/" + speaker->formation_id;
      proposal.speaker_actor_id = speaker->actor_id;
      proposal.stance = speaker->stance;
      proposal.plan = std::move(p.value);
      proposal.text_key = "field.council.proposal." +
                          std::to_string(static_cast<Integer>(speaker->stance));
      out.proposals.push_back(std::move(proposal));
    }
    std::sort(out.proposals.begin(), out.proposals.end(),
              [](const auto &a, const auto &b) {
                return a.proposal_id < b.proposal_id;
              });
  }
  return {true, std::move(result), {}};
}
Outcome<StableId>
select_field_council_proposal(const FieldCouncilSources &sources,
                              const GeneratedBattleField &field,
                              const FieldBattleSession &session, Integer side) {
  if (side < 0 || side > 1)
    return fail<StableId>("InvalidFieldBattleSide");
  const auto &own = sources.sides[side], &enemy = sources.sides[1 - side];
  if (own.fit <= 0 || enemy.fit <= 0)
    return fail<StableId>("FieldCouncilSourceRequired");
  Wide own_height = 0, enemy_height = 0;
  for (auto cell : session.sides[side].deploy_zone) {
    if (cell >= field.cells.size())
      return fail<StableId>("FieldCouncilSourceRequired");
    own_height += field.cells[cell].height_dm;
  }
  for (auto cell : session.sides[1 - side].deploy_zone) {
    if (cell >= field.cells.size())
      return fail<StableId>("FieldCouncilSourceRequired");
    enemy_height += field.cells[cell].height_dm;
  }
  const bool high =
      own_height * enemy.deploy_cells - enemy_height * own.deploy_cells >=
      Wide(300) * own.deploy_cells * enemy.deploy_cells;
  const FieldCouncilProposal *best = nullptr;
  Wide best_score = 0;
  for (const auto &p : session.councils[side].proposals) {
    const auto speaker = std::find_if(
        own.officers.begin(), own.officers.end(),
        [&](const auto &o) { return o.actor_id == p.speaker_actor_id; });
    if (speaker == own.officers.end())
      return fail<StableId>("FieldCouncilSourceRequired", p.speaker_actor_id);
    Integer base = 0;
    if ((p.plan.layout == FieldCouncilLayout::Flank ||
         p.stance == FieldCouncilStance::Aggressive) &&
        Wide(own.fit) * 10 >= Wide(enemy.fit) * 13)
      base += 20;
    if (p.plan.layout == FieldCouncilLayout::Hold &&
        (Wide(own.fit) * 10 < Wide(enemy.fit) * 8 || high))
      base += 20;
    const bool feint = std::any_of(
        p.plan.stratagems.begin(), p.plan.stratagems.end(), [](const auto &s) {
          return s.kind == FieldCouncilStratagemKind::FeintStrength;
        });
    if ((p.plan.layout == FieldCouncilLayout::Ambush || feint) &&
        own.fit < enemy.fit)
      base += 10;
    if (p.plan.layout == FieldCouncilLayout::Ambush &&
        sources.intel[1 - side].level >= 3)
      base -= 40;
    if (p.plan.layout == FieldCouncilLayout::Line)
      base += 5;
    const Wide score = Wide(base) * 10 + speaker->leadership +
                       speaker->intellect + speaker->commander_trust;
    if (!best || score > best_score ||
        (score == best_score && p.proposal_id < best->proposal_id)) {
      best = &p;
      best_score = score;
    }
  }
  if (!best)
    return fail<StableId>("FieldCouncilProposalRequired", session.battle_id);
  return {true, best->proposal_id, {}};
}
Outcome<bool>
approve_field_council_request(const FieldCouncilSources &sources, Integer side,
                              const StableId &actor,
                              const FieldCouncilRequestValue &request) {
  if (side < 0 || side > 1)
    return fail<bool>("InvalidFieldBattleSide");
  const auto &officers = sources.sides[side].officers;
  const auto person =
      std::find_if(officers.begin(), officers.end(),
                   [&](const auto &v) { return v.actor_id == actor; });
  const auto commander =
      std::find_if(officers.begin(), officers.end(), [&](const auto &v) {
        return v.actor_id == sources.sides[side].commander_actor_id;
      });
  if (person == officers.end() || commander == officers.end() ||
      person == commander)
    return fail<bool>("FieldOrderNoAuthority", actor);
  Integer fitness = 0;
  if (const auto *role = std::get_if<FieldCouncilRoleRequest>(&request)) {
    switch (role->role) {
    case FieldCouncilRequestedRole::Vanguard:
      fitness = person->martial >= 70 ? 20 : -20;
      break;
    case FieldCouncilRequestedRole::Flank:
      fitness = person->category == FieldTroopCategory::Cavalry ? 20 : -20;
      break;
    case FieldCouncilRequestedRole::Reserve:
    case FieldCouncilRequestedRole::Guard:
      fitness = 20;
      break;
    default:
      return fail<bool>("InvalidFieldCouncilRequest", actor);
    }
  }
  const Wide twice =
      Wide(person->charisma) + person->commander_trust + 2 * fitness +
      Wide(person->official_rank - commander->official_rank + 2) * 20;
  return {true, twice >= 100, {}};
}
} // namespace hanmo::resident
namespace hanmo::resident {
Status prepare_field_council_adoption(
    FieldBattleSession &session, const GeneratedBattleField &field,
    Integer side, const StableId &proposal_id,
    const std::vector<FieldCouncilAssignment> &overrides) {
  if (side < 0 || side > 1)
    return common_failure("InvalidFieldBattleSide");
  if (session.state != FieldBattleState::Council ||
      session.councils[side].closed)
    return common_failure("FieldCouncilClosed", session.battle_id);
  const auto original =
      std::find_if(session.councils[side].proposals.begin(),
                   session.councils[side].proposals.end(),
                   [&](const auto &p) { return p.proposal_id == proposal_id; });
  if (original == session.councils[side].proposals.end())
    return common_failure("FieldCouncilProposalRequired", proposal_id);
  auto chosen = original->plan;
  std::set<StableId> changed;
  for (const auto &change : overrides) {
    if (!changed.insert(change.formation_id).second)
      return common_failure("InvalidFieldCouncilPlan", change.formation_id);
    auto assignment = std::find_if(
        chosen.assignments.begin(), chosen.assignments.end(),
        [&](const auto &a) { return a.formation_id == change.formation_id; });
    if (assignment == chosen.assignments.end())
      chosen.assignments.push_back(change);
    else
      *assignment = change;
  }
  // 部将准请在正式定案时作用于本部；发言时只记准驳，不提前改战场任务。
  for (const auto &request : session.councils[side].requests) {
    if (request.status != FieldCouncilRequestStatus::Granted)
      continue;
    const auto participant = std::find_if(
        session.councils[side].participants.begin(),
        session.councils[side].participants.end(),
        [&](const auto &p) { return p.actor_id == request.actor_id; });
    if (participant == session.councils[side].participants.end())
      return common_failure("FieldCouncilSourceRequired", request.actor_id);
    auto assignment =
        std::find_if(chosen.assignments.begin(), chosen.assignments.end(),
                     [&](const auto &a) {
                       return a.formation_id == participant->formation_id;
                     });
    if (assignment == chosen.assignments.end())
      return common_failure("FieldCouncilPlanIncomplete",
                            participant->formation_id);
    if (const auto *target =
            std::get_if<FieldCouncilTargetRequest>(&request.value)) {
      const auto enemy = session.formations.find(target->target_formation_id);
      if (enemy == session.formations.end() || enemy->second.side == side)
        return common_failure("InvalidFieldCouncilRequest",
                              target->target_formation_id);
      assignment->mission = FieldEngage{target->target_formation_id};
      assignment->flank_followup = false;
      assignment->wave = FieldCouncilWave::First;
    } else {
      const auto role = std::get<FieldCouncilRoleRequest>(request.value).role;
      assignment->flank_followup = role == FieldCouncilRequestedRole::Flank;
      const auto own = session.formations.find(participant->formation_id);
      if (own == session.formations.end())
        return common_failure("FieldCouncilSourceRequired",
                              participant->formation_id);
      if (role == FieldCouncilRequestedRole::Flank) {
        assignment->wave = FieldCouncilWave::First;
      } else if (role == FieldCouncilRequestedRole::Vanguard) {
        FieldCouncilOfficerSource speaker;
        speaker.actor_id = request.actor_id;
        speaker.formation_id = participant->formation_id;
        speaker.stance = FieldCouncilStance::Aggressive;
        auto vanguard = plan(session, field, side, speaker);
        if (!vanguard.ok)
          return {false, {}, vanguard.error};
        const auto mission =
            std::find_if(vanguard.value.assignments.begin(),
                         vanguard.value.assignments.end(), [&](const auto &a) {
                           return a.formation_id == participant->formation_id;
                         });
        if (mission == vanguard.value.assignments.end())
          return common_failure("FieldCouncilPlanIncomplete",
                                participant->formation_id);
        *assignment = *mission;
      } else if (role == FieldCouncilRequestedRole::Reserve) {
        // 实际位置随后采用同一布局，不借未摆位时的默认坐标。
        assignment->wave = FieldCouncilWave::Reserve;
        assignment->mission = FieldHold{};
      } else if (role == FieldCouncilRequestedRole::Guard) {
        const FieldFormation *commander = nullptr;
        for (const auto &[id, f] : session.formations)
          if (f.side == side &&
              f.commander_actor_id == session.sides[side].commander_actor_id)
            commander = &f;
        if (!commander)
          return common_failure("FieldCouncilSourceRequired", request.actor_id);
        assignment->mission = FieldSupport{commander->formation_id};
        assignment->wave = FieldCouncilWave::First;
      } else {
        return common_failure("InvalidFieldCouncilRequest", request.actor_id);
      }
    }
  }
  // 准入选型必须有限；不把非法枚举转成默认 line。
  FieldAiStrategy strategy;
  switch (chosen.layout) {
  case FieldCouncilLayout::Hold:
    strategy = FieldAiStrategy::HoldGround;
    break;
  case FieldCouncilLayout::Line:
  case FieldCouncilLayout::Ambush:
    strategy = FieldAiStrategy::LineReserve;
    break;
  case FieldCouncilLayout::Flank:
    strategy = FieldAiStrategy::Flanking;
    break;
  default:
    return common_failure("InvalidFieldCouncilPlan", proposal_id);
  }
  auto layout = prepare_field_deployment_layout(session, field, side, strategy);
  if (!layout.ok)
    return {false, {}, layout.error};
  for (auto &assignment : chosen.assignments) {
    const auto placed =
        std::find_if(layout.value.placements.begin(),
                     layout.value.placements.end(), [&](const auto &p) {
                       return p.formation_id == assignment.formation_id;
                     });
    if (placed == layout.value.placements.end())
      return common_failure("FieldCouncilPlanIncomplete",
                            assignment.formation_id);
    const bool commander_vanguard =
        placed->commander &&
        original->speaker_actor_id == session.sides[side].commander_actor_id &&
        original->stance == FieldCouncilStance::Aggressive;
    if ((placed->commander && !commander_vanguard) ||
        assignment.wave == FieldCouncilWave::Reserve) {
      assignment.mission = FieldHold{placed->point_dm, placed->facing_mdeg};
      assignment.flank_followup = false;
    } else if (assignment.flank_followup) {
      auto flank = read_field_flank_target(session, field, side, *placed);
      if (!flank.ok)
        return {false, {}, flank.error};
      assignment.mission = flank.value;
    }
  }
  std::set<StableId> assigned;
  for (const auto &a : chosen.assignments) {
    auto f = session.formations.find(a.formation_id);
    if (f == session.formations.end() || f->second.side != side ||
        !assigned.insert(a.formation_id).second ||
        static_cast<Integer>(a.wave) < 0 || static_cast<Integer>(a.wave) > 2)
      return common_failure("InvalidFieldCouncilPlan", a.formation_id);
    auto valid =
        validate_field_battle_mission(a.mission, f->second, session, field);
    if (!valid.ok)
      return valid;
    if (a.wave == FieldCouncilWave::Reserve &&
        !std::holds_alternative<FieldHold>(a.mission))
      return common_failure("InvalidFieldCouncilPlan", a.formation_id);
  }
  for (const auto &[id, f] : session.formations)
    if (f.side == side && !assigned.contains(id))
      return common_failure("FieldCouncilPlanIncomplete", id);
  // 34c0d0ee: recheck against the actually adopted assignments, after all
  // commander overrides and granted roles. Preserve voided attachment history.
  Integer accepted_stratagems = 0;
  for (auto &item : chosen.stratagems) {
    item.voided_at_adoption =
        !field_council_stratagem_assignment_eligible(item, chosen);
    if (!item.voided_at_adoption)
      ++accepted_stratagems;
  }
  if (accepted_stratagems &&
      (!session.sides[side].strategist_actor_id ||
       session.sides[side].stratagem_points < accepted_stratagems))
    return common_failure("FieldStratagemPointsRequired", proposal_id);
  // 所有来源/任务先核完再动准备对象；外层事务仍是唯一发布边界。
  for (const auto &p : layout.value.placements) {
    auto &f = session.formations.at(p.formation_id);
    const auto a = std::find_if(
        chosen.assignments.begin(), chosen.assignments.end(),
        [&](const auto &a) { return a.formation_id == p.formation_id; });
    if (a == chosen.assignments.end())
      return common_failure("FieldCouncilPlanIncomplete", p.formation_id);
    f.position_dm = p.point_dm;
    f.facing_mdeg = p.facing_mdeg;
    // A selected, eligible ambush is deployed at its actual assigned Hold
    // area, not hidden at the generic line slot while marching towards it.
    for (const auto &item : chosen.stratagems)
      if (!item.voided_at_adoption &&
          item.kind == FieldCouncilStratagemKind::Ambush &&
          item.formation_id == f.formation_id) {
        const auto &hold = std::get<FieldHold>(a->mission);
        f.position_dm = hold.point_dm;
        f.facing_mdeg = hold.facing_mdeg;
      }
    f.flank_assignment_index =
        a->flank_followup
            ? std::optional<Integer>{Integer(a - chosen.assignments.begin())}
            : std::nullopt;
    f.wave = a->wave;
    if (a->wave == FieldCouncilWave::Second) {
      f.wave_mission = a->mission;
      f.mission = FieldHold{p.point_dm, p.facing_mdeg};
    } else {
      f.wave_mission.reset();
      f.mission = a->mission;
    }
    f.state = std::holds_alternative<FieldHold>(f.mission)
                  ? FieldFormationState::Holding
                  : FieldFormationState::Moving;
    Integer morale = f.morale_start;
    if (f.commander_actor_id == original->speaker_actor_id)
      morale = std::min<Integer>(100, morale + 5);
    else
      for (const auto &proposal : session.councils[side].proposals)
        if (proposal.stance == FieldCouncilStance::Aggressive &&
            proposal.speaker_actor_id == f.commander_actor_id)
          morale = std::max<Integer>(0, morale - 3);
    f.morale = f.morale_start = morale;
  }
  original->plan = std::move(chosen);
  auto &council = session.councils[side];
  council.adopted_proposal_id = proposal_id;
  council.closed = true;
  if (session.councils[1 - side].intel.level >= 3)
    session.councils[1 - side].intel.enemy_plan = original->plan;
  if (session.councils[0].closed && session.councils[1].closed) {
    // §4 只有玩家主将进入可微调 Deploying；玩家部将听令后由 AI 当即布阵开战。
    const bool player = std::any_of(
        session.sides.begin(), session.sides.end(), [](const auto &s) {
          return std::holds_alternative<FieldPlayerController>(s.controller);
        });
    session.state =
        player ? FieldBattleState::Deploying : FieldBattleState::Running;
    if (!player)
      session.started_at = session.now;
  }
  return common_success();
}
} // namespace hanmo::resident
