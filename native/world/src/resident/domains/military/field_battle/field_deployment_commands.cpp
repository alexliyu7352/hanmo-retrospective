#include "field_event_legacy_kinds.hpp"
#include "field_contact_origin.hpp"
// 同一 Field 根的真实布阵写者；权限复用原只读叶，几何复用原生成器，不落旧棋盘。
#include "field_binding_source.hpp"
#include "field_command_source.hpp"
#include "field_council_prepare.hpp"
#include "field_council_validation.hpp"
#include "field_deployment.hpp"
#include "field_deployment_commands.hpp"
#include "field_exact_contact_source.hpp"
#include "field_integer_geometry.hpp"
#include "field_local_control_loss.hpp"
#include "field_order_resolution.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_session_build.hpp"
#include "field_stratagem_resolution.hpp"
#include "field_view_model.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <tuple>

namespace hanmo::resident {
namespace {
// 只投影原权限叶实际消费的四字段；不存在 hex/chart
// 位置输入、旧会战安装或旧回合。
Outcome<MilitaryBattleControl>
control(const CommonStore &common, const MilitaryState &military,
        const MilitaryCatalog &catalog, const FieldBattleSession &session,
        const FieldBattleWorldState &world, const StableId &actor) {
  const auto *person = common.actor(actor);
  if (!person || !person->alive || !person->available)
    return {false, {}, common_failure("FieldOrderNoAuthority", actor).error};
  const FieldBattleRoadBinding *actual_binding = nullptr;
  for (const auto &[id, binding] : world.army_bindings)
    for (const auto &member : binding.members)
      if (member.actor_id == actor) {
        if (actual_binding || binding.battle_id != session.battle_id)
          return {
              false, {}, common_failure("FieldOrderNoAuthority", actor).error};
        actual_binding = &binding;
      }
  if (!actual_binding)
    return {false, {}, common_failure("FieldOrderNoAuthority", actor).error};
  std::vector<BattleUnit> units;
  units.reserve(session.formations.size());
  for (const auto &[id, formation] : session.formations) {
    BattleUnit unit;
    unit.unit_id = id;
    unit.army_id = formation.army_id;
    unit.commander_actor_id = formation.commander_actor_id;
    unit.side = formation.side;
    units.push_back(std::move(unit));
  }
  return military_battle_control(common, military, catalog, units, actor);
}
// ade07cdf §6.1: only Order may borrow the original army authority of a
// player commander who is still in the actual entrance queue. This is not
// membership in the battlefield and grants no Direct/tactic/other-army access.
Outcome<MilitaryBattleControl> queued_player_order_control(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleCatalog &field_catalog,
    const FieldBattleSession &session, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleAbilityReadContext &sources) {
  const auto fail = [&]() -> Outcome<MilitaryBattleControl> {
    return {
        false, {}, common_failure("FieldQueuedOrderNoAuthority", actor).error};
  };
  if (actor != common.state().metadata.player_actor_id ||
      !sources.exact_contacts || !sources.original_rosters ||
      &sources.exact_contacts->common != &common ||
      &sources.exact_contacts->military != &military ||
      &sources.exact_contacts->field != &world)
    return fail();
  // An actor already admitted must use the ordinary control path, never this
  // exception after losing a current battlefield binding.
  for (const auto &[id, formation] : session.formations) {
    if (formation.commander_actor_id == actor)
      return fail();
    for (const auto *members :
         {&formation.fit, &formation.wounded, &formation.dead,
          &formation.captured, &formation.deserted})
      if (std::find(members->begin(), members->end(), actor) != members->end())
        return fail();
  }
  const FieldBattleBoundaryRecord *actual = nullptr;
  for (const auto &[id, boundary] : world.boundary_contacts) {
    if (boundary.battle_id != session.battle_id || !boundary.army_id ||
        !boundary.side || !boundary.reinforcement_queue ||
        boundary.status != FieldBoundaryStatus::Entered ||
        boundary.reinforcement_queue->admitted.empty() ||
        boundary.reinforcement_queue->admitted.size() >=
            boundary.reinforcement_queue->formation_ids.size())
      continue;
    const auto army =
        std::find_if(military.armies.begin(), military.armies.end(),
                     [&](const auto &a) { return a.id == *boundary.army_id; });
    if (army == military.armies.end() || army->commander_actor_id != actor)
      continue;
    if (actual || std::count(boundary.participant_actor_ids.begin(),
                             boundary.participant_actor_ids.end(), actor) != 1)
      return fail();
    auto queue = read_field_original_reinforcement_queue(
        *sources.exact_contacts, field_catalog, *sources.original_rosters,
        {"military", "field_battle_contact", id});
    if (!queue.ok)
      return {false, {}, queue.error};
    actual = &boundary;
  }
  if (!actual)
    return fail();
  std::vector<BattleUnit> units;
  for (const auto &entry : actual->reinforcement_queue->admitted) {
    const auto formation = session.formations.find(entry.formation_id);
    if (formation == session.formations.end() ||
        formation->second.army_id != *actual->army_id ||
        formation->second.side != *actual->side)
      return fail();
    auto identity = read_field_reinforcement_admission_identity(
        common, world, session, formation->second);
    if (!identity.ok)
      return {false, {}, identity.error};
    BattleUnit unit;
    unit.unit_id = formation->first;
    unit.army_id = formation->second.army_id;
    unit.commander_actor_id = formation->second.commander_actor_id;
    unit.side = formation->second.side;
    units.push_back(std::move(unit));
  }
  auto authority =
      military_battle_control(common, military, catalog, units, actor);
  if (!authority.ok)
    return authority;
  if (authority.value.scope != "army" ||
      authority.value.army_id != *actual->army_id ||
      authority.value.side != *actual->side)
    return fail();
  return authority;
}
// Internal AI after the actual player's death is a system continuation of
// this battle side, not permission for a dead actor to submit commands.
Outcome<MilitaryBattleControl>
command_control(const CommonStore &common, const MilitaryState &military,
                const MilitaryCatalog &catalog,
                const FieldBattleSession &session,
                const FieldBattleWorldState &world, const StableId &actor,
                FieldControlCommandOrigin origin,
                const FieldBattleAbilityReadContext &sources) {
  if (origin != FieldControlCommandOrigin::Ai)
    return control(common, military, catalog, session, world, actor);
  auto fail = [&]() -> Outcome<MilitaryBattleControl> {
    return {false, {}, common_failure("FieldOrderNoAuthority", actor).error};
  };
  Integer side = -1;
  for (Integer i = 0; i < 2; ++i)
    if (session.sides[i].commander_actor_id == actor) {
      if (side != -1)
        return fail();
      side = i;
    }
  auto permission = validate_field_current_ai_side_authority(
      common, world, session, side, actor, sources.original_life);
  if (!permission.ok)
    return {false, {}, permission.error};
  MilitaryBattleControl out;
  out.actor_id = actor;
  out.scope = "army";
  out.side = side;
  for (const auto &[id, formation] : session.formations) {
    if (formation.side != side || formation.fit.empty() ||
        formation.state == FieldFormationState::Withdrawn ||
        formation.state == FieldFormationState::Destroyed)
      continue;
    const auto binding = world.army_bindings.find(formation.army_id);
    if (binding == world.army_bindings.end() ||
        binding->second.battle_id != session.battle_id)
      return fail();
    auto actual = read_field_original_battle_binding(
        common, world, session.battle_id, formation.army_id);
    if (!actual.ok)
      return {false, {}, actual.error};
    auto permitted = read_field_current_ai_formation_permission(
        common, world, session, formation, sources.original_life);
    if (!permitted.ok)
      return {false, {}, permitted.error};
    if (!permitted.value)
      continue;
    out.unit_ids.push_back(id);
    if (formation.commander_actor_id == actor)
      out.army_id = formation.army_id;
  }
  if (out.unit_ids.empty())
    return fail();
  return {true, std::move(out), {}};
}
// 真实 Contact 保留原几何和时间出处；本次命令不能传入第二套战场。
Outcome<GeneratedBattleField>
generated(const TravelCatalog &travel, const FieldBattleCatalog &catalog,
          const FieldBattleWorldState &world, const StableId &battle_id,
          const FieldBattleAbilityReadContext &abilities) {
  const FieldBattleContactRecord *source = nullptr;
  for (const auto &[id, contact] : world.contacts)
    if (contact.battle_id == battle_id) {
      if (source)
        return {false,
                {},
                common_failure("FieldContactSourceMismatch", battle_id).error};
      source = &contact;
    }
  if (!source)
    return {false,
            {},
            common_failure("FieldContactSourceRequired", battle_id).error};
  return regenerate_field_contact_battle(travel, catalog, source->source,
                                         abilities.exact_contacts);
}
// 连续分米位置只用于定位原格与碰撞；不把命令坐标量化为棋格中心。
Status placement(const FieldFormation &formation,
                 const FieldBattleSession &session,
                 const GeneratedBattleField &field) {
  const auto point = formation.position_dm;
  const auto &p = field.parameters;
  if (point.x < -p.length_m * 5 || point.x >= p.length_m * 5 ||
      point.y < -p.width_m * 5 || point.y >= p.width_m * 5)
    return common_failure("FieldPositionOutsideZone", formation.formation_id);
  const Integer x = (point.x + p.length_m * 5) / (p.cell_m * 10);
  const Integer y = (point.y + p.width_m * 5) / (p.cell_m * 10);
  const auto index = static_cast<std::size_t>(y * field.columns + x);
  if (index >= field.cells.size() ||
      !field_cell_passable(field.cells[index].terrain))
    return common_failure("FieldPositionBlocked", formation.formation_id);
  const auto &zone = session.sides[formation.side].deploy_zone;
  if (std::find(zone.begin(), zone.end(), index) == zone.end())
    return common_failure("FieldPositionOutsideZone", formation.formation_id);
  const Integer radius = field_formation_radius_dm(formation);
  for (const auto &[id, other] : session.formations) {
    if (id == formation.formation_id ||
        other.state == FieldFormationState::Deploying)
      continue;
    const Integer sum = radius + field_formation_radius_dm(other);
    if (field_distance_squared(point, other.position_dm) <
        __int128_t(sum) * sum)
      return common_failure("FieldPositionBlocked", formation.formation_id);
  }
  return common_success();
}
// 指定只可引用本编队现有具名成员，不能借副手字段移动他军人物或创建一条继任链。
Status deputy(const CommonStore &common, const FieldFormation &formation) {
  if (!formation.deputy_actor_id)
    return common_success();
  const auto &id = *formation.deputy_actor_id;
  const auto *actor = common.actor(id);
  const bool belongs =
      std::find(formation.fit.begin(), formation.fit.end(), id) !=
          formation.fit.end() ||
      std::find(formation.wounded.begin(), formation.wounded.end(), id) !=
          formation.wounded.end();
  if (!actor || !actor->alive || !actor->available || !belongs ||
      id == formation.commander_actor_id)
    return common_failure("FieldOrderNoAuthority", id);
  return common_success();
}
} // namespace

Status prepare_field_battle_deployment(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleSetDeployment &command,
    FieldFormation &prepared, const FieldBattleAbilityReadContext &abilities) {
  const auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", command.battle_id);
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &abilities);
  if (!valid.ok)
    return valid;
  auto &session = found->second;
  if (session.state != FieldBattleState::Deploying)
    return common_failure("FieldBattleNotDeploying", command.battle_id);
  const auto scope =
      control(common, military, military_catalog, session, world, actor);
  if (!scope.ok)
    return {false, {}, scope.error};
  const auto current = session.formations.find(command.formation_id);
  if (current == session.formations.end() ||
      std::find(scope.value.unit_ids.begin(), scope.value.unit_ids.end(),
                command.formation_id) == scope.value.unit_ids.end() ||
      scope.value.side != current->second.side)
    return common_failure("FieldOrderNoAuthority", command.formation_id);
  if (command.facing_mdeg < 0 || command.facing_mdeg >= 360000 ||
      (command.control != FieldDeploymentControl::Direct &&
       command.control != FieldDeploymentControl::Autonomous))
    return common_failure("InvalidFieldDeployment", command.formation_id);
  // 本人所在编队保 Direct；可指定的其它中军仍不得越过原军级/本部权限。
  if (current->second.commander_actor_id == actor &&
      command.control != FieldDeploymentControl::Direct)
    return common_failure("FieldOrderNoAuthority", command.formation_id);
  if (current->second.mission_revision == std::numeric_limits<Integer>::max())
    return common_failure("FieldMissionRevisionStale", command.formation_id);
  auto field = generated(travel, catalog, world, command.battle_id, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  // 预备波仍守阵，不能通过后续摆位绕过军议任务限制。
  if (current->second.wave == FieldCouncilWave::Reserve &&
      !std::holds_alternative<FieldHold>(command.mission))
    return common_failure("InvalidFieldCouncilPlan", command.formation_id);
  auto valid_mission = validate_field_battle_mission(
      command.mission, current->second, session, field.value);
  if (!valid_mission.ok)
    return valid_mission;
  FieldFormation next = current->second;
  next.flank_assignment_index.reset();
  next.position_dm = command.position_dm;
  next.facing_mdeg = command.facing_mdeg;
  if (next.wave && *next.wave == FieldCouncilWave::Second) {
    next.wave_mission = command.mission;
    next.mission = FieldHold{command.position_dm, command.facing_mdeg};
  } else {
    next.wave_mission.reset();
    next.mission = command.mission;
  }
  next.control = command.control == FieldDeploymentControl::Direct
                     ? FieldFormationControl{FieldDirectControl{actor}}
                     : FieldFormationControl{FieldAutonomousControl{}};
  next.deputy_actor_id = command.deputy_actor_id;
  next.state = std::holds_alternative<FieldHold>(next.mission)
                   ? FieldFormationState::Holding
                   : FieldFormationState::Moving;
  ++next.mission_revision;
  valid = placement(next, session, field.value);
  if (!valid.ok)
    return valid;
  valid =
      validate_field_battle_mission(next.mission, next, session, field.value);
  if (!valid.ok)
    return valid;
  valid = deputy(common, next);
  if (!valid.ok)
    return valid;
  prepared = std::move(next);
  return common_success();
}

Status validate_field_battle_start(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleStart &command,
    const FieldBattleAbilityReadContext &abilities) {
  const auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", command.battle_id);
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &abilities);
  if (!valid.ok)
    return valid;
  auto &session = found->second;
  if (session.state != FieldBattleState::Deploying)
    return common_failure("FieldBattleNotDeploying", command.battle_id);
  const auto scope =
      control(common, military, military_catalog, session, world, actor);
  if (!scope.ok)
    return {false, {}, scope.error};
  if (scope.value.unit_ids.empty())
    return common_failure("FieldOrderNoAuthority", actor);
  for (const auto &[id, formation] : session.formations)
    if (formation.side == scope.value.side &&
        formation.state == FieldFormationState::Deploying)
      return common_failure("FieldBattleDeploymentIncomplete", id);
  return validate_field_battle_start_ability_sources(common, abilities,
                                                     session);
}
// 真正写入口只采用刚验证的同一候选，不调用只读预览期间的另一份世界。
Status set_field_battle_deployment_in_candidate(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleSetDeployment &command,
    const FieldBattleAbilityReadContext &abilities) {
  FieldFormation prepared;
  auto valid = prepare_field_battle_deployment(
      common, military, military_catalog, travel, catalog, world, actor,
      command, prepared, abilities);
  if (!valid.ok)
    return valid;
  world.sessions.at(command.battle_id).formations.at(command.formation_id) =
      std::move(prepared);
  return common_success();
}
Status start_field_battle_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleStart &command,
    const FieldBattleAbilityReadContext &abilities,
    CheckpointMemoryCharge &owner) {
  auto valid =
      validate_field_battle_start(common, military, military_catalog, travel,
                                  catalog, world, actor, command, abilities);
  if (!valid.ok)
    return valid;
  auto &session = world.sessions.at(command.battle_id);
  // 此头就在真实初始化之前，原 Common append 次序是冷读资格截断点。
  // 后续任何失败仍由外层 World 候选整体撤回，不能发布只有头的半 Running。
  auto id = common.next_id("field_start");
  if (!id.ok)
    return {false, {}, id.error};
  auto appended =
      common.append_fact({id.value, "field_battle_started", "military"});
  if (!appended.ok)
    return appended;
  return initialize_field_battle_abilities(
      common, abilities, session,
      {"military", "field_battle_started", id.value}, common.tick(), actor,
      owner);
}
} // namespace hanmo::resident
namespace hanmo::resident {
Status prepare_field_council_speak(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldCouncilSpeak &command,
    FieldBattleSession &prepared,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources) {
  const auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", command.battle_id);
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &abilities);
  if (!valid.ok)
    return valid;
  const auto &session = found->second;
  if (session.state != FieldBattleState::Council)
    return common_failure("FieldCouncilClosed", command.battle_id);
  auto authority =
      control(common, military, military_catalog, session, world, actor);
  if (!authority.ok)
    return {false, {}, authority.error};
  const auto side = authority.value.side;
  if (side < 0 || side > 1 || session.councils[side].closed)
    return common_failure("FieldCouncilClosed", command.battle_id);
  const auto participant =
      std::find_if(session.councils[side].participants.begin(),
                   session.councils[side].participants.end(),
                   [&](const auto &p) { return p.actor_id == actor; });
  if (participant == session.councils[side].participants.end() ||
      actor != common.state().metadata.player_actor_id)
    return common_failure("FieldOrderNoAuthority", actor);
  if (session.councils[side].player_actor_id != actor)
    return common_failure("FieldOrderNoAuthority", actor);
  const bool commander = session.sides[side].commander_actor_id == actor;
  const auto *plan = std::get_if<FieldCouncilPlan>(&command.speech);
  if (commander && (!plan || authority.value.scope != "army"))
    return common_failure("InvalidFieldCouncilRequest", actor);
  if (!commander && plan)
    return common_failure("FieldCouncilNotCommander", actor);
  if (!commander && !session.councils[side].requests.empty())
    return common_failure("FieldCouncilAlreadySpoken", actor);
  auto field = generated(travel, catalog, world, command.battle_id, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  auto next = session;
  if (commander) {
    FieldCouncilProposal proposal;
    proposal.proposal_id =
        command.battle_id + "/proposal/player/" +
        std::to_string(next.councils[side].proposals.size() + 1);
    proposal.speaker_actor_id = actor;
    proposal.stance = participant->stance;
    proposal.plan = *plan;
    proposal.text_key = "field.council.proposal.player";
    next.councils[side].proposals.push_back(std::move(proposal));
  } else {
    FieldCouncilRequest request;
    request.actor_id = actor;
    if (const auto *role =
            std::get_if<FieldCouncilRoleRequest>(&command.speech)) {
      request.value = *role;
    } else {
      const auto &target = std::get<FieldCouncilTargetRequest>(command.speech);
      const auto f = session.formations.find(target.target_formation_id);
      if (f == session.formations.end() || f->second.side == side)
        return common_failure("InvalidFieldCouncilRequest",
                              target.target_formation_id);
      request.value = target;
    }
    auto sources = read_field_council_sources(
        common, session, field.value, council_sources.social, military,
        catalog.office_titles, council_sources.organization, abilities);
    if (!sources.ok)
      return {false, {}, sources.error};
    const auto approved = approve_field_council_request(sources.value, side,
                                                        actor, request.value);
    if (!approved.ok)
      return {false, {}, approved.error};
    request.status = approved.value ? FieldCouncilRequestStatus::Granted
                                    : FieldCouncilRequestStatus::Denied;
    request.reason = approved.value
                         ? "主将准许本次自请。"
                         : "主将认为本次自请把握不足，仍按定案行事。";
    next.councils[side].requests.push_back(std::move(request));
  }
  auto shape = validate_field_council_records(common, next, field.value);
  if (!shape.ok)
    return shape;
  prepared = std::move(next);
  return common_success();
}
Status prepare_field_council_adopt(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldCouncilAdopt &command,
    FieldBattleSession &prepared,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources) {
  const auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", command.battle_id);
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &abilities);
  if (!valid.ok)
    return valid;
  const auto &session = found->second;
  if (session.state != FieldBattleState::Council)
    return common_failure("FieldCouncilClosed", command.battle_id);
  auto authority =
      control(common, military, military_catalog, session, world, actor);
  if (!authority.ok)
    return {false, {}, authority.error};
  const auto side = authority.value.side;
  if (side < 0 || side > 1 || session.councils[side].closed)
    return common_failure("FieldCouncilClosed", command.battle_id);
  if (actor != common.state().metadata.player_actor_id ||
      session.councils[side].player_actor_id != actor)
    return common_failure("FieldOrderNoAuthority", actor);
  const bool commander = session.sides[side].commander_actor_id == actor;
  if (commander && (authority.value.scope != "army" || !command.proposal_id))
    return common_failure("FieldCouncilProposalRequired", actor);
  if (!commander && (command.proposal_id || !command.overrides.empty() ||
                     !std::holds_alternative<FieldAiController>(
                         session.sides[side].controller)))
    return common_failure("FieldCouncilNotCommander", actor);
  auto field = generated(travel, catalog, world, command.battle_id, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  auto next = session;
  StableId proposal;
  if (commander) {
    proposal = *command.proposal_id;
  } else {
    auto sources = read_field_council_sources(
        common, session, field.value, council_sources.social, military,
        catalog.office_titles, council_sources.organization, abilities);
    if (!sources.ok)
      return {false, {}, sources.error};
    auto selected = select_field_council_proposal(sources.value, field.value,
                                                  session, side);
    if (!selected.ok)
      return {false, {}, selected.error};
    proposal = std::move(selected.value);
  }
  valid = prepare_field_council_adoption(next, field.value, side, proposal,
                                         command.overrides);
  if (!valid.ok)
    return valid;
  valid = validate_field_council_records(common, next, field.value);
  if (!valid.ok)
    return valid;
  if (next.state == FieldBattleState::Running) {
    // 部将的零时听令也必须先证明开战能力，预览不能假报可采用。
    valid =
        validate_field_battle_start_ability_sources(common, abilities, next);
    if (!valid.ok)
      return valid;
  }
  prepared = std::move(next);
  return common_success();
}
Status speak_field_council_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, const FieldCouncilSpeak &command,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources) {
  FieldBattleSession next;
  auto valid = prepare_field_council_speak(
      common, military, military_catalog, travel, catalog, world, actor,
      command, next, abilities, council_sources);
  if (!valid.ok)
    return valid;
  if (!std::holds_alternative<FieldCouncilPlan>(command.speech)) {
    FieldBattleEvent event;
    bool found = false;
    for (Integer side = 0; side < 2; ++side) {
      const auto &council = next.councils[side];
      if (council.player_actor_id != actor || council.requests.size() != 1)
        continue;
      const auto &request = council.requests.front();
      for (const auto &participant : council.participants)
        if (participant.actor_id == actor) {
          event.formation_id = participant.formation_id;
          found = true;
        }
      event.side = side;
      event.kind = request.status == FieldCouncilRequestStatus::Granted
                       ? field_event_kind::RequestGranted
                       : field_event_kind::RequestDenied;
    }
    if (!found)
      return common_failure("FieldCouncilSourceRequired", actor);
    valid = append_field_battle_event(next, std::move(event));
    if (!valid.ok)
      return valid;
  }
  world.sessions.at(command.battle_id) = std::move(next);
  return common_success();
}
Status adopt_field_council_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, const FieldCouncilAdopt &command,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources,
    CheckpointMemoryCharge &owner) {
  FieldBattleSession next;
  auto valid = prepare_field_council_adopt(
      common, military, military_catalog, travel, catalog, world, actor,
      command, next, abilities, council_sources);
  if (!valid.ok)
    return valid;
  Integer side = -1;
  for (Integer i = 0; i < 2; ++i)
    if (next.councils[i].player_actor_id == actor)
      side = i;
  if (side < 0)
    return common_failure("FieldCouncilNotCommander", actor);
  auto id = common.next_id("field_council");
  if (!id.ok)
    return {false, {}, id.error};
  FieldCouncilFact fact;
  fact.fact_id = id.value;
  fact.battle_id = next.battle_id;
  fact.side = side;
  fact.at = next.now;
  fact.recorded_tick = common.tick();
  fact.kind = FieldCouncilFactKind::Adopted;
  auto adoption_moment =
      read_field_actual_application_moment(common, abilities);
  if (!adoption_moment.ok)
    return {false, {}, adoption_moment.error};
  fact.adopted_actual_at = adoption_moment.value;
  fact.council = next.councils[side];
  valid = common.append_fact({id.value, "military_council", "military"});
  if (!valid.ok)
    return valid;
  if (!world.council_facts.emplace(id.value, std::move(fact)).second)
    return common_failure("FieldBattleIdRepeated", id.value,
                          ErrorKind::internal);
  FieldBattleEvent event;
  event.kind = field_event_kind::CouncilAdopted;
  event.side = side;
  event.source_id = id.value;
  valid = append_field_battle_event(next, std::move(event));
  if (!valid.ok)
    return valid;
  if (next.state == FieldBattleState::Running) {
    // AI 主将的玩家部将听令是零时实际开战，不借最初 Session 的旧能力截面。
    // 真正开始头与能力冻结属于这同一个候选，后续失败由外层一并撤回。
    auto start = common.next_id("field_start");
    if (!start.ok)
      return {false, {}, start.error};
    valid =
        common.append_fact({start.value, "field_battle_started", "military"});
    if (!valid.ok)
      return valid;
    valid = initialize_field_battle_abilities(
        common, abilities, next,
        {"military", "field_battle_started", start.value}, common.tick(), actor,
        owner);
    if (!valid.ok)
      return valid;
  }
  world.sessions.at(command.battle_id) = std::move(next);
  auto field = generated(travel, catalog, world, command.battle_id, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  return install_field_adopted_stratagems_in_candidate(
      common, world, id.value, field.value, abilities, owner);
}
} // namespace hanmo::resident
namespace hanmo::resident {
namespace {
// 准备结果只借同一次世界中的现存记录；不创建会话、副本兵员或第二个命令账。
struct PreparedFieldControl {
  const FieldBattleSession *session = nullptr;
  const FieldFormation *formation = nullptr;
  Integer side = 0;
  std::optional<FieldOrderRecord> order;
};
// 比较局部时间只用合同 BattleTime，不读取第二种世界时钟。
Integer elapsed_field_seconds(const FieldBattleTime &from,
                              const FieldBattleTime &to,
                              const FieldBattleParameters &parameters) {
  const auto value =
      (__int128_t(to.tick) - from.tick) * parameters.window_s + to.t - from.t;
  if (value < 0 || value > std::numeric_limits<Integer>::max())
    return -1;
  return static_cast<Integer>(value);
}
// 发令者所在编队必须是真实在场具名成员；出场/覆灭者不能作为近距离归队锚。
const FieldFormation *field_issuer_formation(const FieldBattleSession &session,
                                             const StableId &actor) {
  for (const auto &[id, f] : session.formations) {
    if (f.state == FieldFormationState::Withdrawn ||
        f.state == FieldFormationState::Destroyed)
      continue;
    if (f.commander_actor_id == actor || f.deputy_actor_id == actor ||
        std::find(f.fit.begin(), f.fit.end(), actor) != f.fit.end() ||
        std::find(f.wounded.begin(), f.wounded.end(), actor) != f.wounded.end())
      return &f;
  }
  return nullptr;
}
// 传令延迟只在发令时计算；已存在 due 不因移动、暂停或冷恢复重算。
Outcome<FieldOrderRecord> prepare_field_mission_order(
    const FieldBattleSession &session, const FieldFormation &recipient,
    const StableId &actor, const FieldOrderPayload &payload,
    const GeneratedBattleField &field) {
  const auto &p = field.parameters;
  if (p.courier_m_per_minute <= 0 || p.minimum_order_delay_s < 0)
    return {false, {}, common_failure("InvalidFieldBattleParameters").error};
  const auto *issuer = field_issuer_formation(session, actor);
  const auto origin = issuer ? issuer->position_dm
                             : field.sides[recipient.side].deployment_center_dm;
  const auto square = field_distance_squared(origin, recipient.position_dm);
  const Integer distance_m =
      field_integer_sqrt(static_cast<__uint128_t>(square)) / 10;
  const auto raw = (__int128_t(distance_m) * 60 + p.courier_m_per_minute - 1) /
                   p.courier_m_per_minute;
  auto delay = std::max<__int128_t>(p.minimum_order_delay_s, raw);
  // StaffWork 取本侧真实统帅编队已冻结的当前指挥者特性；不借逝者今日能力。
  for (const auto &[id, f] : session.formations)
    if (f.side == recipient.side &&
        f.commander_actor_id ==
            session.sides[recipient.side].commander_actor_id &&
        f.ability &&
        std::find(f.ability->traits.begin(), f.ability->traits.end(),
                  FieldTrait::StaffWork) != f.ability->traits.end())
      delay = (delay * 3 + 3) / 4;
  if (delay > std::numeric_limits<Integer>::max())
    return {false, {}, common_failure("FieldBattleTimeOverflow").error};
  auto due =
      add_field_battle_seconds(session.now, static_cast<Integer>(delay), p);
  if (!due.ok)
    return {false, {}, due.error};
  FieldOrderRecord order;
  order.issuer_actor_id = actor;
  order.target_formation_id = recipient.formation_id;
  order.payload = payload;
  order.issued = session.now;
  order.due = due.value;
  order.delay_s = static_cast<Integer>(delay);
  order.distance_m = distance_m;
  return {true, std::move(order), {}};
}
// 真实权限/绑定在先；玩家参数不能借 AI 来源越过 Direct 或一次委托限制。
Outcome<PreparedFieldControl> prepare_field_control(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, Integer request_sequence,
    const FieldDeploymentCommand &command, FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities) {
  auto fail = [&](const char *code,
                  const StableId &id) -> Outcome<PreparedFieldControl> {
    return {false, {}, common_failure(code, id).error};
  };
  const bool player = origin == FieldControlCommandOrigin::Player;
  if ((!player && origin != FieldControlCommandOrigin::Ai) ||
      (player && (request_sequence < 0 ||
                  common.state().metadata.player_actor_id != actor)))
    return fail("FieldOrderNoAuthority", actor);
  if (!player && !std::holds_alternative<FieldBattleDirect>(command) &&
      !std::holds_alternative<FieldBattleOrder>(command))
    return fail("FieldOrderNoAuthority", actor);
  const auto battle =
      std::visit([](const auto &c) { return c.battle_id; }, command);
  const auto found = world.sessions.find(battle);
  if (found == world.sessions.end())
    return fail("FieldBattleNotFound", battle);
  // Public commands enter from a fully published World. Native AI commands
  // are nested original phases of one unpublished candidate; their own real
  // authority/source/mission gates below still apply, and the complete outer
  // World validator remains mandatory before publication.
  if (origin == FieldControlCommandOrigin::Player) {
    auto valid = validate_field_battle_world(common, military, travel, catalog,
                                             world, &abilities);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  const auto &session = found->second;
  if (session.state != FieldBattleState::Running)
    return fail("FieldBattleNotRunning", battle);
  auto authority = command_control(common, military, military_catalog, session,
                                   world, actor, origin, abilities);
  if (!authority.ok && player &&
      std::holds_alternative<FieldBattleOrder>(command))
    authority =
        queued_player_order_control(common, military, military_catalog, catalog,
                                    session, world, actor, abilities);
  if (!authority.ok)
    return {false, {}, authority.error};
  const Integer side = authority.value.side;
  if (side < 0 || side > 1 ||
      (player && session.sides[side].delegated_by_actor_id == actor) ||
      (!player && (!std::holds_alternative<FieldAiController>(
                       session.sides[side].controller) ||
                   session.sides[side].commander_actor_id != actor)))
    return fail("FieldOrderNoAuthority", actor);
  auto field = generated(travel, catalog, world, battle, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  PreparedFieldControl prepared;
  prepared.session = &session;
  prepared.side = side;
  return std::visit(
      [&](const auto &c) -> Outcome<PreparedFieldControl> {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, FieldBattleDelegate>) {
          const auto *controller = std::get_if<FieldPlayerController>(
              &session.sides[side].controller);
          if (!player || !controller || controller->actor_id != actor ||
              session.sides[side].delegated_by_actor_id ||
              authority.value.scope != "army")
            return fail("FieldOrderNoAuthority", actor);
          return {true, prepared, {}};
        } else if constexpr (std::is_same_v<T, FieldBattleDirect> ||
                             std::is_same_v<T, FieldBattleOrder> ||
                             std::is_same_v<T, FieldBattleDispatch> ||
                             std::is_same_v<T, FieldBattleRejoin>) {
          const auto f = session.formations.find(c.formation_id);
          if (f == session.formations.end() || f->second.side != side ||
              std::find(authority.value.unit_ids.begin(),
                        authority.value.unit_ids.end(),
                        c.formation_id) == authority.value.unit_ids.end())
            return fail("FieldOrderNoAuthority", c.formation_id);
          const auto &formation = f->second;
          if (formation.pending_player_entry_order && !player) {
            auto permitted = read_field_current_ai_formation_permission(
                common, world, session, formation, abilities.original_life);
            if (!permitted.ok)
              return {false, {}, permitted.error};
            if (!permitted.value)
              return fail("FieldReinforcementPlayerTaskRequired",
                          c.formation_id);
          }
          if (formation.state == FieldFormationState::Withdrawn ||
              formation.state == FieldFormationState::Destroyed)
            return fail("FieldFormationUnavailable", c.formation_id);
          prepared.formation = &formation;
          const auto *direct =
              std::get_if<FieldDirectControl>(&formation.control);
          if constexpr (std::is_same_v<T, FieldBattleDirect> ||
                        std::is_same_v<T, FieldBattleDispatch>) {
            if (!direct || direct->controller_actor_id != actor)
              return fail("FieldFormationNotDirect", c.formation_id);
            if (formation.mission_revision ==
                std::numeric_limits<Integer>::max())
              return fail("FieldMissionRevisionStale", c.formation_id);
            if constexpr (std::is_same_v<T, FieldBattleDirect>)
              if (c.expected_mission_revision != formation.mission_revision)
                return fail("FieldMissionRevisionStale", c.formation_id);
          } else if constexpr (std::is_same_v<T, FieldBattleOrder>) {
            // AI 主将给玩家部将的令是 §5.3 的明确例外，到达只 Delivered。
            const bool player_subordinate =
                !player && direct &&
                direct->controller_actor_id ==
                    session.councils[side].player_actor_id &&
                direct->controller_actor_id != actor;
            if (direct && !player_subordinate)
              return fail("FieldFormationNotAutonomous", c.formation_id);
          } else {
            if (direct)
              return fail("FieldFormationNotAutonomous", c.formation_id);
            const auto *issuer = field_issuer_formation(session, actor);
            if (!issuer || formation.state == FieldFormationState::Engaged ||
                formation.state == FieldFormationState::Routing ||
                !formation.stopped_since ||
                elapsed_field_seconds(*formation.stopped_since, session.now,
                                      catalog.parameters) <
                    catalog.parameters.rejoin_stopped_s ||
                field_distance_squared(formation.position_dm,
                                       issuer->position_dm) >
                    __int128_t(catalog.parameters.rejoin_distance_m) * 10 *
                        catalog.parameters.rejoin_distance_m * 10)
              return fail("FieldRejoinNotReady", c.formation_id);
          }
          if constexpr (!std::is_same_v<T, FieldBattleRejoin>) {
            auto valid = validate_field_battle_mission(c.mission, formation,
                                                       session, field.value);
            if (!valid.ok)
              return {false, {}, valid.error};
            if constexpr (std::is_same_v<T, FieldBattleOrder>) {
              auto order = prepare_field_mission_order(
                  session, formation, actor, c.mission, field.value);
              if (!order.ok)
                return {false, {}, order.error};
              prepared.order = std::move(order.value);
            }
          }
          return {true, std::move(prepared), {}};
        } else {
          return fail("InvalidFieldControlCommand", battle);
        }
      },
      command);
}
// 部将的实际下令另记永久 MilitaryCouncilFact，任务载荷不依赖稍后可变编队。
Status append_field_player_order_fact(CommonStore &common,
                                      FieldBattleWorldState &world,
                                      const FieldBattleSession &session,
                                      Integer side, const StableId &actor,
                                      const FieldDeploymentCommand &command,
                                      FieldBattleMutationResult &result) {
  if (session.councils[side].player_actor_id != actor ||
      session.sides[side].commander_actor_id == actor)
    return common_success();
  auto id = common.next_id("field_council");
  if (!id.ok)
    return {false, {}, id.error};
  FieldCouncilFact fact;
  fact.fact_id = id.value;
  fact.battle_id = session.battle_id;
  fact.side = side;
  fact.at = session.now;
  fact.recorded_tick = common.tick();
  fact.kind = FieldCouncilFactKind::PlayerOrder;
  fact.player_actor_id = actor;
  fact.player_command_kind = field_deployment_command_kind(command);
  fact.formation_id = result.formation_id;
  fact.mission_revision = result.mission_revision;
  fact.order_id = result.order_id;
  fact.tactic_id = result.tactic_id;
  fact.activation_id = result.activation_id;
  std::visit(
      [&](const auto &c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, FieldBattleDirect> ||
                      std::is_same_v<T, FieldBattleOrder> ||
                      std::is_same_v<T, FieldBattleDispatch>)
          fact.player_mission = c.mission;
      },
      command);
  auto appended =
      common.append_fact({id.value, "military_council", "military"});
  if (!appended.ok)
    return appended;
  if (!world.council_facts.emplace(id.value, std::move(fact)).second)
    return common_failure("FieldBattleIdRepeated", id.value,
                          ErrorKind::internal);
  result.council_fact_id = id.value;
  return common_success();
}
// Direct/Order/Tactic use a real retained council acceptance head; AI never
// fabricates a player CommandResult. Original order/activation remains owner.
Status append_field_ai_command_fact(CommonStore &common,
                                    FieldBattleWorldState &world,
                                    FieldBattleSession &session, Integer side,
                                    const StableId &actor,
                                    const FieldDeploymentCommand &command,
                                    FieldBattleMutationResult &result) {
  const auto kind = field_deployment_command_kind(command);
  if (kind != "field_battle_direct" && kind != "field_battle_order" &&
      kind != "field_battle_tactic")
    return common_failure("FieldAiCommandKindInvalid");
  if (!result.formation_id ||
      !session.formations.contains(*result.formation_id))
    return common_failure("FieldAiFormationSourceRequired");
  auto &formation = session.formations.at(*result.formation_id);
  if (kind == "field_battle_direct" &&
      (!formation.pending_direct || !formation.pending_direct->ai_issued ||
       formation.pending_direct->command_result ||
       formation.pending_direct->ai_source_id))
    return common_failure("FieldAiDirectSourceMismatch", *result.formation_id);
  auto id = common.next_id("field_council");
  if (!id.ok)
    return {false, {}, id.error};
  FieldCouncilFact fact;
  fact.fact_id = id.value;
  fact.battle_id = session.battle_id;
  fact.side = side;
  fact.at = session.now;
  fact.recorded_tick = common.tick();
  fact.kind = FieldCouncilFactKind::AiOrder;
  if (!result.actual_control_acceptance)
    return common_failure("FieldAiAcceptanceBoundaryRequired",
                          session.battle_id);
  fact.ai_control_acceptance = result.actual_control_acceptance;
  fact.player_actor_id = actor;
  fact.player_command_kind = kind;
  fact.formation_id = result.formation_id;
  fact.mission_revision = result.mission_revision;
  fact.order_id = result.order_id;
  fact.tactic_id = result.tactic_id;
  fact.activation_id = result.activation_id;
  std::visit(
      [&](const auto &c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, FieldBattleDirect> ||
                      std::is_same_v<T, FieldBattleOrder>)
          fact.player_mission = c.mission;
      },
      command);
  auto added = common.append_fact({id.value, "military_council", "military"});
  if (!added.ok)
    return added;
  if (!world.council_facts.emplace(id.value, std::move(fact)).second)
    return common_failure("FieldBattleIdRepeated", id.value,
                          ErrorKind::internal);
  if (kind == "field_battle_direct")
    formation.pending_direct->ai_source_id = id.value;
  result.council_fact_id = id.value;
  return common_success();
}
} // namespace
Status validate_field_battle_control_command(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, Integer request_sequence,
    const FieldDeploymentCommand &command, FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities) {
  auto prepared = prepare_field_control(
      common, military, military_catalog, travel, catalog, world, actor,
      request_sequence, command, origin, abilities);
  return prepared.ok ? common_success() : Status{false, {}, prepared.error};
}
Status apply_field_battle_control_command_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, Integer request_sequence,
    const FieldDeploymentCommand &command, FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities,
    FieldBattleMutationResult &result) {
  auto prepared = prepare_field_control(
      common, military, military_catalog, travel, catalog, world, actor,
      request_sequence, command, origin, abilities);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  auto &session = world.sessions.at(prepared.value.session->battle_id);
  const Integer side = prepared.value.side;
  if (!abilities.actual_clock || !abilities.original_read_boundary)
    return common_failure("FieldControlAcceptanceWorldBoundaryRequired",
                          session.battle_id);
  auto acceptance = capture_field_control_acceptance(
      common, session, *abilities.actual_clock,
      *abilities.original_read_boundary,
      origin == FieldControlCommandOrigin::Player);
  if (!acceptance.ok)
    return {false, {}, acceptance.error};
  FieldBattleMutationResult applied;
  applied.actual_control_acceptance = std::move(acceptance.value);
  applied.battle_id = session.battle_id;
  applied.state = session.state;
  applied.at = session.now;
  auto changed = std::visit(
      [&](const auto &c) -> Status {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, FieldBattleDelegate>) {
          session.sides[side].controller = FieldAiController{};
          session.sides[side].delegated_by_actor_id = actor;
          for (auto &[id, f] : session.formations)
            if (f.side == side &&
                std::holds_alternative<FieldDirectControl>(f.control) &&
                f.commander_actor_id !=
                    session.sides[side].commander_actor_id) {
              f.control = FieldAutonomousControl{};
              f.control_command_result.reset();
            }
          return common_success();
        } else if constexpr (std::is_same_v<T, FieldBattleDirect> ||
                             std::is_same_v<T, FieldBattleOrder> ||
                             std::is_same_v<T, FieldBattleDispatch> ||
                             std::is_same_v<T, FieldBattleRejoin>) {
          auto &formation = session.formations.at(c.formation_id);
          applied.formation_id = c.formation_id;
          if constexpr (!std::is_same_v<T, FieldBattleRejoin>) {
            applied.mission = c.mission;
            applied.effective_at = session.now;
          }
          if constexpr (std::is_same_v<T, FieldBattleDirect>) {
            ++formation.mission_revision;
            FieldPendingDirectMission pending;
            pending.issuer_actor_id = actor;
            pending.ai_issued = origin == FieldControlCommandOrigin::Ai;
            pending.request_sequence = request_sequence;
            pending.accepted_revision = formation.mission_revision;
            pending.mission = c.mission;
            // now 表示下一个尚未执行的步起点；零时提交不先跑这一步。
            pending.apply_at = session.now;
            formation.pending_direct = std::move(pending);
            applied.mission_revision = formation.mission_revision;
          } else if constexpr (std::is_same_v<T, FieldBattleOrder>) {
            auto id = common.next_id("field_order");
            if (!id.ok)
              return {false, {}, id.error};
            auto order = std::move(*prepared.value.order);
            order.order_id = id.value;
            order.ai_issued = origin == FieldControlCommandOrigin::Ai;
            if (!session.orders.emplace(id.value, std::move(order)).second)
              return common_failure("FieldBattleIdRepeated", id.value,
                                    ErrorKind::internal);
            applied.order_id = id.value;
            applied.effective_at = session.orders.at(id.value).due;
          } else if constexpr (std::is_same_v<T, FieldBattleDispatch>) {
            formation.control = FieldAutonomousControl{session.now};
            formation.mission = c.mission;
            formation.pending_player_entry_order = false;
            formation.mission_command_result.reset();
            formation.mission_transition_sequence.reset();
            formation.mission_ai_source_id.reset();
            if (formation.state != FieldFormationState::Routing &&
                formation.state != FieldFormationState::Engaged)
              formation.state = std::holds_alternative<FieldHold>(c.mission)
                                    ? FieldFormationState::Holding
                                    : FieldFormationState::Moving;
            formation.stopped_since.reset();
            formation.hold_since.reset();
            formation.pending_direct.reset();
            formation.wave_mission.reset();
            formation.followup_mission.reset();
            formation.adopted_order_id.reset();
            formation.navigation_target_dm.reset();
            formation.navigation_observation_sequence.reset();
            formation.navigation_replanned_at.reset();
            formation.ai_pursuit = false;
            ++formation.mission_revision;
            applied.mission_revision = formation.mission_revision;
          } else {
            formation.control = FieldDirectControl{actor};
          }
          return common_success();
        } else {
          return common_failure("InvalidFieldControlCommand",
                                session.battle_id);
        }
      },
      command);
  if (!changed.ok)
    return changed;
  if (origin == FieldControlCommandOrigin::Player) {
    changed = append_field_player_order_fact(common, world, session, side,
                                             actor, command, applied);
    if (!changed.ok)
      return changed;
  }
  if (origin == FieldControlCommandOrigin::Ai) {
    changed = append_field_ai_command_fact(common, world, session, side, actor,
                                           command, applied);
    if (!changed.ok)
      return changed;
  }
  result = std::move(applied);
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
// due 落在半开步 [from,to)；合同规定在该步起点处理，不改写发令时冻结的 due。
bool field_before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
bool same_field_time(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
// 任务采用只替换任务及其缓存依据，不瞬移，也不把溃退/接战状态抹成行军。
void replace_field_mission(FieldFormation &formation,
                           const FieldMission &mission,
                           const FieldBattleTime &at, bool pursuit) {
  formation.mission = mission;
  formation.pending_player_entry_order = false;
  formation.mission_ai_source_id.reset();
  formation.wave_mission.reset();
  formation.followup_mission.reset();
  formation.navigation_target_dm.reset();
  formation.navigation_observation_sequence.reset();
  formation.navigation_replanned_at.reset();
  formation.hold_since.reset();
  formation.ai_pursuit = pursuit;
  if (formation.state != FieldFormationState::Routing &&
      formation.state != FieldFormationState::Engaged) {
    formation.state = std::holds_alternative<FieldHold>(mission)
                          ? FieldFormationState::Holding
                          : FieldFormationState::Moving;
    if (formation.state == FieldFormationState::Holding)
      formation.hold_since = at;
  }
  // stopped_since 记录真实位移，不能在只是接新任务时倒填已停留的 300 秒。
}
// 上级命令到达玩家部将只形成真实军议事实，不改玩家任务、不扣关系或加处罚。
Status append_field_delivered_order_fact(CommonStore &common,
                                         FieldBattleWorldState &world,
                                         const FieldBattleSession &session,
                                         const FieldOrderRecord &order,
                                         const FieldFormation &formation,
                                         const FamilyLifeHistoryRead4 *life) {
  const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
  if (!direct || !order.status_at || !order.resolution_boundary ||
      session.sides[formation.side].commander_actor_id ==
          direct->controller_actor_id)
    return common_failure("FieldOrderDeliverySourceRequired", order.order_id);
  auto player = read_field_original_player_control_at_boundary(
      common, world, session, formation, direct->controller_actor_id,
      *order.resolution_boundary, life);
  if (!player.ok)
    return {false, {}, player.error};
  if (!player.value)
    return common_failure("FieldOrderDeliverySourceRequired", order.order_id);
  for (const auto &[id, fact] : world.council_facts)
    if (fact.kind == FieldCouncilFactKind::OrderDelivered &&
        fact.battle_id == session.battle_id && fact.order_id == order.order_id)
      return common_failure("FieldOrderAlreadyDelivered", order.order_id);
  auto id = common.next_id("field_council");
  if (!id.ok)
    return {false, {}, id.error};
  FieldCouncilFact fact;
  fact.fact_id = id.value;
  fact.battle_id = session.battle_id;
  fact.side = formation.side;
  fact.at = *order.status_at;
  fact.recorded_tick = common.tick();
  fact.kind = FieldCouncilFactKind::OrderDelivered;
  fact.order_id = order.order_id;
  fact.player_actor_id = direct->controller_actor_id;
  fact.formation_id = formation.formation_id;
  auto appended =
      common.append_fact({id.value, "military_council", "military"});
  if (!appended.ok)
    return appended;
  if (!world.council_facts.emplace(id.value, std::move(fact)).second)
    return common_failure("FieldBattleIdRepeated", id.value,
                          ErrorKind::internal);
  return common_success();
}
} // namespace
Status apply_field_control_arrivals_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const StableId &battle_id, const FieldBattleParameters &parameters,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &world_owner,
    const FamilyLifeHistoryRead4 *original_life,
    const CurrentMomentRead *actual_clock,
    const OriginalWorldReadBoundary *actual_boundary) {
  auto found = world.sessions.find(battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", battle_id);
  auto &session = found->second;
  auto allowed = read_field_battle_window_slice(session, common.tick(), step.to,
                                                parameters);
  if (!allowed.ok)
    return {false, {}, allowed.error};
  if (step.steps != 1 || allowed.value.steps != 1 ||
      !same_field_time(step.from, session.now) ||
      !same_field_time(step.from, allowed.value.from) ||
      step.segment != session.segment || step.opens_next_segment ||
      session.window_complete)
    return common_failure("InvalidFieldBattleStep", battle_id);
  // 先核全部输入来源再消费任何令；真正军令/部署权限仍由原接受入口完成。
  for (const auto &[id, formation] : session.formations)
    if (formation.pending_direct &&
        (!same_field_time(formation.pending_direct->apply_at, step.from) ||
         formation.pending_direct->ai_issued ==
             bool(formation.pending_direct->command_result) ||
         formation.pending_direct->ai_issued !=
             bool(formation.pending_direct->ai_source_id) ||
         formation.pending_direct->accepted_revision !=
             formation.mission_revision))
      return common_failure("FieldPendingDirectSourceRequired", id);
  for (const auto &[id, formation] : session.formations) {
    if (!formation.pending_direct || !formation.pending_direct->ai_issued)
      continue;
    const auto &pending = *formation.pending_direct;
    auto source = read_field_ai_direct_source(
        common, world, *pending.ai_source_id, original_life);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &fact = *source.value;
    if (fact.battle_id != battle_id || fact.formation_id != id ||
        fact.player_actor_id != pending.issuer_actor_id ||
        fact.mission_revision != pending.accepted_revision ||
        fact.ai_pursuit != pending.ai_pursuit ||
        !same_field_battle_mission(*fact.player_mission, pending.mission) ||
        fact.at.tick != pending.apply_at.tick ||
        fact.at.t != pending.apply_at.t)
      return common_failure("FieldPendingDirectSourceRequired", id);
    const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
    if (formation.side < 0 || formation.side > 1 || !direct ||
        direct->controller_actor_id != pending.issuer_actor_id ||
        session.sides[formation.side].commander_actor_id !=
            pending.issuer_actor_id ||
        !std::holds_alternative<FieldAiController>(
            session.sides[formation.side].controller))
      return common_failure("FieldPendingDirectSourceRequired", id);
  }
  std::vector<StableId> due;
  for (const auto &[id, order] : session.orders) {
    if (order.status != FieldOrderStatus::InTransit)
      continue;
    if (field_before(order.due, step.from))
      return common_failure("FieldOrderMissedStep", id);
    if (!field_before(order.due, step.to))
      continue;
    auto target = session.formations.find(order.target_formation_id);
    if (target == session.formations.end())
      return common_failure("FieldOrderSourceRequired", id);
    if (target->second.mission_revision == std::numeric_limits<Integer>::max())
      return common_failure("FieldMissionRevisionStale", target->first);
    auto report =
        add_field_battle_seconds(step.from, order.delay_s, parameters);
    if (!report.ok)
      return {false, {}, report.error};
    due.push_back(id);
  }
  std::sort(due.begin(), due.end(), [&](const auto &a, const auto &b) {
    const auto &x = session.orders.at(a), &y = session.orders.at(b);
    return std::tie(x.due.tick, x.due.t, x.order_id) <
           std::tie(y.due.tick, y.due.t, y.order_id);
  });
  for (auto &[id, formation] : session.formations) {
    if (!formation.pending_direct)
      continue;
    const auto pending = std::move(*formation.pending_direct);
    replace_field_mission(formation, pending.mission, step.from,
                          pending.ai_pursuit);
    formation.adopted_order_id.reset();
    formation.mission_transition_sequence.reset();
    formation.mission_command_result = pending.command_result;
    formation.mission_ai_source_id = pending.ai_source_id;
    formation.pending_direct.reset();
    auto applied = record_field_actual_mission_application(
        common, world, session, formation, world_owner, original_life);
    if (!applied.ok)
      return applied;
  }
  for (const auto &id : due) {
    auto &order = session.orders.at(id);
    auto &formation = session.formations.at(order.target_formation_id);
    if (order.resolution_event_sequence || order.resolution_boundary)
      return common_failure("FieldOrderResolutionRepeated", id);
    if (!actual_clock || !actual_boundary)
      return common_failure("FieldOrderResolutionWorldBoundaryRequired", id);
    auto resolution_boundary = capture_field_control_acceptance(
        common, session, *actual_clock, *actual_boundary, false);
    if (!resolution_boundary.ok)
      return {false, {}, resolution_boundary.error};
    order.resolution_boundary = std::move(resolution_boundary.value);
    auto resolved = [&]() -> Status {
      order.status_at = step.from;
      auto report =
          add_field_battle_seconds(step.from, order.delay_s, parameters);
      if (!report.ok)
        return {false, {}, report.error};
      order.report_due = report.value;
      order.reason.clear();
      order.superseded_by.clear();
      if (formation.state == FieldFormationState::Withdrawn ||
          formation.state == FieldFormationState::Destroyed) {
        order.status = FieldOrderStatus::Undeliverable;
        order.reason = formation.state == FieldFormationState::Withdrawn
                           ? "FieldFormationWithdrawn"
                           : "FieldFormationDestroyed";
        return common_success();
      }
      const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
      bool player_direct = false;
      if (direct) {
        auto player = read_field_original_player_control_at_boundary(
            common, world, session, formation, direct->controller_actor_id,
            *order.resolution_boundary, original_life);
        if (!player.ok)
          return {false, {}, player.error};
        player_direct = player.value;
      }
      if (player_direct) {
        order.status = FieldOrderStatus::Delivered;
        if (session.sides[formation.side].commander_actor_id !=
            direct->controller_actor_id) {
          auto recorded = append_field_delivered_order_fact(
              common, world, session, order, formation, original_life);
          if (!recorded.ok)
            return recorded;
        }
        return common_success();
      }
      const auto commander = read_field_effective_commander(common, formation);
      const bool confused =
          std::any_of(formation.statuses.begin(), formation.statuses.end(),
                      [&](const auto &status) {
                        return status.kind == FieldStatusKind::Confused &&
                               status.until &&
                               field_before(step.from, *status.until);
                      });
      if (!commander.ok || formation.state == FieldFormationState::Routing ||
          formation.morale < 25 || confused) {
        order.status = FieldOrderStatus::NotResponding;
        order.reason = !commander.ok ? "FieldTacticNoCommander"
                       : formation.state == FieldFormationState::Routing
                           ? "FieldFormationRouting"
                       : formation.morale < 25 ? "FieldFormationLowMorale"
                                               : "FieldFormationConfused";
        return common_success();
      }
      if (const auto *intent = std::get_if<FieldTacticIntent>(&order.payload)) {
        auto ready = validate_field_tactic_activation(
            common, formation, session.now, intent->tactic_id, parameters);
        if (!ready.ok) {
          const auto &code = ready.error.original.code;
          if (code != "FieldTacticUnknown" && code != "FieldTacticNoPoints" &&
              code != "FieldTacticCooling" &&
              code != "FieldTacticNoCommander" &&
              code != "FieldFormationRouting")
            return ready;
          order.status = FieldOrderStatus::NotResponding;
          order.reason = code;
          return common_success();
        }
        order.status = FieldOrderStatus::Adopted;
        auto activated = activate_field_tactic_in_candidate(
            common, session, formation, order.issuer_actor_id,
            intent->tactic_id, &order.order_id, order.ai_issued, parameters,
            world_owner);
        if (!activated.ok)
          return {false, {}, activated.error};
        return common_success();
      }
      order.status = FieldOrderStatus::Adopted;
      replace_field_mission(formation, std::get<FieldMission>(order.payload),
                            step.from, order.ai_pursuit);
      formation.pending_direct.reset();
      formation.mission_command_result.reset();
      formation.mission_transition_sequence.reset();
      formation.adopted_order_id = id;
      ++formation.mission_revision;
      order.adopted_mission_revision = formation.mission_revision;
      auto applied = record_field_actual_mission_application(
          common, world, session, formation, world_owner, original_life);
      if (!applied.ok)
        return applied;
      // 先采用过的令仍记录 Adopted{at}；当前任务只指后来实际采用的那个令。
      // 不把后来替换的信息倒写到旧令更早的 report_due，制造提前知情。
      return common_success();
    }();
    if (!resolved.ok)
      return resolved;
    auto recorded = record_field_order_resolution(session, order, world_owner);
    if (!recorded.ok)
      return recorded;
  }
  return common_success();
}
Status publish_field_order_reports_in_candidate(FieldBattleSession &session) {
  // 仅由真实 driver 在完成一步、now 已移到新边界后调用，Read 不执行此写入。
  for (const auto &[id, order] : session.orders) {
    if (!order.report_due || field_before(session.now, *order.report_due))
      continue;
    if (order.status == FieldOrderStatus::InTransit || !order.status_at)
      return common_failure("FieldOrderReportSourceRequired", id);
    bool published = false;
    for (const auto &event : session.events)
      if (event.kind == field_event_kind::OrderArrived &&
          event.source_id == id) {
        if (published)
          return common_failure("FieldOrderReportRepeated", id);
        published = true;
      }
    if (published)
      continue;
    const auto recipient = session.formations.find(order.target_formation_id);
    if (recipient == session.formations.end())
      return common_failure("FieldOrderSourceRequired", id);
    FieldBattleEvent event;
    event.kind = field_event_kind::OrderArrived;
    event.formation_id = order.target_formation_id;
    event.source_id = id;
    event.side = recipient->second.side;
    auto appended = append_field_battle_event(session, std::move(event));
    if (!appended.ok)
      return appended;
  }
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
struct PreparedFieldTacticCommand {
  const FieldBattleSession *session = nullptr;
  const FieldFormation *formation = nullptr;
  std::optional<FieldOrderRecord> order;
};
// 发令与发动分开：自主编队到达时再核点数/冷却，不能发令即扣后又重复扣。
Outcome<PreparedFieldTacticCommand> prepare_field_tactic_command(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleTactic &command,
    FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities) {
  auto fail = [&](const char *code) -> Outcome<PreparedFieldTacticCommand> {
    return {false, {}, common_failure(code, command.formation_id).error};
  };
  if (origin != FieldControlCommandOrigin::Player &&
      origin != FieldControlCommandOrigin::Ai)
    return fail("FieldOrderNoAuthority");
  if (origin == FieldControlCommandOrigin::Player &&
      common.state().metadata.player_actor_id != actor)
    return fail("FieldOrderNoAuthority");
  auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return fail("FieldBattleNotFound");
  // Public commands enter from a fully published World. Native AI commands
  // are nested original phases of one unpublished candidate; their own real
  // authority/source/mission gates below still apply, and the complete outer
  // World validator remains mandatory before publication.
  if (origin == FieldControlCommandOrigin::Player) {
    auto valid = validate_field_battle_world(common, military, travel, catalog,
                                             world, &abilities);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  const auto &session = found->second;
  if (session.state != FieldBattleState::Running)
    return fail("FieldBattleNotRunning");
  auto target = session.formations.find(command.formation_id);
  if (target == session.formations.end())
    return fail("FieldFormationUnavailable");
  auto authority = command_control(common, military, military_catalog, session,
                                   world, actor, origin, abilities);
  if (!authority.ok)
    return {false, {}, authority.error};
  const auto &formation = target->second;
  const auto side = formation.side;
  if (authority.value.side != side ||
      std::find(authority.value.unit_ids.begin(),
                authority.value.unit_ids.end(),
                command.formation_id) == authority.value.unit_ids.end() ||
      (origin == FieldControlCommandOrigin::Player &&
       session.sides[side].delegated_by_actor_id == actor) ||
      (origin == FieldControlCommandOrigin::Ai &&
       (!std::holds_alternative<FieldAiController>(
            session.sides[side].controller) ||
        session.sides[side].commander_actor_id != actor)))
    return fail("FieldOrderNoAuthority");
  if (formation.state == FieldFormationState::Withdrawn ||
      formation.state == FieldFormationState::Destroyed)
    return fail("FieldFormationUnavailable");
  const auto commander = read_field_effective_commander(common, formation);
  if (!commander.ok)
    return {false, {}, commander.error};
  if (!formation.ability ||
      formation.ability->commander_actor_id != commander.value)
    return fail("FieldAbilityCommanderSourceRequired");
  const auto known = std::find_if(
      formation.ability->tactics.begin(), formation.ability->tactics.end(),
      [&](const auto &t) { return t.source.tactic_id == command.tactic_id; });
  if (known == formation.ability->tactics.end())
    return fail("FieldTacticUnknown");
  const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
  PreparedFieldTacticCommand prepared{&session, &formation, {}};
  if (direct && direct->controller_actor_id == actor) {
    auto valid = validate_field_tactic_activation(
        common, formation, session.now, command.tactic_id, catalog.parameters);
    if (!valid.ok)
      return {false, {}, valid.error};
    return {true, prepared, {}};
  }
  if (direct &&
      !(origin == FieldControlCommandOrigin::Ai &&
        session.councils[side].player_actor_id == direct->controller_actor_id &&
        direct->controller_actor_id != actor))
    return fail("FieldFormationNotAutonomous");
  auto field = generated(travel, catalog, world, command.battle_id, abilities);
  if (!field.ok)
    return {false, {}, field.error};
  auto order = prepare_field_mission_order(session, formation, actor,
                                           FieldTacticIntent{command.tactic_id},
                                           field.value);
  if (!order.ok)
    return {false, {}, order.error};
  prepared.order = std::move(order.value);
  return {true, std::move(prepared), {}};
}
} // namespace
Status validate_field_battle_tactic_command(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleTactic &command,
    FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities) {
  auto prepared = prepare_field_tactic_command(
      common, military, military_catalog, travel, catalog, world, actor,
      command, origin, abilities);
  return prepared.ok ? common_success() : Status{false, {}, prepared.error};
}
Status apply_field_battle_tactic_command_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleTactic &command,
    FieldControlCommandOrigin origin,
    const FieldBattleAbilityReadContext &abilities,
    CheckpointMemoryCharge &owner, FieldBattleMutationResult &result) {
  auto prepared = prepare_field_tactic_command(
      common, military, military_catalog, travel, catalog, world, actor,
      command, origin, abilities);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  auto &session = world.sessions.at(command.battle_id);
  auto &formation = session.formations.at(command.formation_id);
  if (!abilities.actual_clock || !abilities.original_read_boundary)
    return common_failure("FieldControlAcceptanceWorldBoundaryRequired",
                          session.battle_id);
  auto acceptance = capture_field_control_acceptance(
      common, session, *abilities.actual_clock,
      *abilities.original_read_boundary,
      origin == FieldControlCommandOrigin::Player);
  if (!acceptance.ok)
    return {false, {}, acceptance.error};
  FieldBattleMutationResult applied;
  applied.actual_control_acceptance = std::move(acceptance.value);
  applied.battle_id = command.battle_id;
  applied.state = session.state;
  applied.at = session.now;
  applied.formation_id = command.formation_id;
  applied.tactic_id = command.tactic_id;
  const bool ai = origin == FieldControlCommandOrigin::Ai;
  if (prepared.value.order) {
    auto id = common.next_id("field_order");
    if (!id.ok)
      return {false, {}, id.error};
    auto order = std::move(*prepared.value.order);
    order.order_id = id.value;
    order.ai_issued = ai;
    applied.order_id = id.value;
    applied.effective_at = order.due;
    if (!session.orders.emplace(id.value, std::move(order)).second)
      return common_failure("FieldBattleIdRepeated", id.value,
                            ErrorKind::internal);
  } else {
    auto activated = activate_field_tactic_in_candidate(
        common, session, formation, actor, command.tactic_id, nullptr, ai,
        catalog.parameters, owner);
    if (!activated.ok)
      return {false, {}, activated.error};
    applied.activation_id = std::move(activated.value);
    applied.effective_at = session.now;
  }
  if (!ai) {
    auto recorded = append_field_player_order_fact(
        common, world, session, formation.side, actor, command, applied);
    if (!recorded.ok)
      return recorded;
  }
  if (ai) {
    auto recorded = append_field_ai_command_fact(
        common, world, session, formation.side, actor, command, applied);
    if (!recorded.ok)
      return recorded;
  }
  result = std::move(applied);
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
struct PreparedFieldConfuse {
  Integer side = 0, chance_quarters = 0;
  StableId strategist, target_commander;
  FieldBattleTime due;
};
Outcome<PreparedFieldConfuse> prepare_field_confuse(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &commands, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleStratagem &command,
    const FieldBattleAbilityReadContext &sources) {
  auto fail = [&](const char *code) -> Outcome<PreparedFieldConfuse> {
    return {false, {}, common_failure(code, command.battle_id).error};
  };
  if (command.kind != FieldStratagemKind::Confuse ||
      !command.target_formation_id || command.formation_id || command.area_dm ||
      command.proposal_id)
    return fail("InvalidFieldStratagem");
  const auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return fail("FieldBattleNotFound");
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &session = found->second;
  if (session.state != FieldBattleState::Running)
    return fail("FieldBattleNotRunning");
  if (actor != common.state().metadata.player_actor_id)
    return fail("FieldOrderNoAuthority");
  auto permission = control(common, military, commands, session, world, actor);
  if (!permission.ok)
    return {false, {}, permission.error};
  const auto side = permission.value.side;
  if (side < 0 || side > 1 || permission.value.scope != "army" ||
      session.sides[side].commander_actor_id != actor ||
      !std::holds_alternative<FieldPlayerController>(
          session.sides[side].controller))
    return fail("FieldOrderNoAuthority");
  const auto &own = session.sides[side];
  if (!own.strategist_actor_id)
    return fail("FieldStratagemNoStrategist");
  const auto *strategist = common.actor(*own.strategist_actor_id);
  if (!strategist || !strategist->alive || !strategist->available)
    return fail("FieldStratagemNoStrategist");
  bool present = false;
  for (const auto &participant : session.councils[side].participants)
    if (participant.role == FieldCouncilRole::Strategist &&
        participant.actor_id == *own.strategist_actor_id) {
      const auto f = session.formations.find(participant.formation_id);
      if (f == session.formations.end() || f->second.side != side ||
          f->second.state == FieldFormationState::Withdrawn ||
          f->second.state == FieldFormationState::Destroyed)
        continue;
      auto effective = read_field_effective_commander(common, f->second);
      if (effective.ok && effective.value == *own.strategist_actor_id)
        present = true;
    }
  if (!present)
    return fail("FieldStratagemNoStrategist");
  if (own.stratagem_points <= 0)
    return fail("FieldStratagemPointsRequired");
  const auto target = session.formations.find(*command.target_formation_id);
  if (target == session.formations.end() || target->second.side == side ||
      target->second.fit.empty() ||
      target->second.state == FieldFormationState::Withdrawn ||
      target->second.state == FieldFormationState::Destroyed)
    return fail("FieldStratagemTargetUnseen");
  for (const auto &status : target->second.statuses)
    if (status.kind == FieldStatusKind::Hidden)
      return fail("FieldStratagemTargetUnseen");
  bool range = false;
  for (const auto &[id, f] : session.formations)
    if (f.side == side && !f.fit.empty() &&
        f.state != FieldFormationState::Withdrawn &&
        f.state != FieldFormationState::Destroyed &&
        field_distance_squared(f.position_dm, target->second.position_dm) <=
            __int128_t(6000) * 6000)
      range = true;
  if (!range)
    return fail("FieldStratagemTargetUnseen");
  auto target_actor = read_field_effective_commander(common, target->second);
  if (!target_actor.ok)
    return {false, {}, target_actor.error};
  auto a = read_field_officer_statistics(
      common, sources, *own.strategist_actor_id, common.tick());
  auto b = read_field_officer_statistics(common, sources, target_actor.value,
                                         common.tick());
  if (!a.ok || !b.ok)
    return {false, {}, !a.ok ? a.error : b.error};
  auto due = add_field_battle_seconds(session.now, 120, catalog.parameters);
  if (!due.ok)
    return {false, {}, due.error};
  const auto chance =
      std::clamp<__int128_t>(200 +
                                 (__int128_t(a.value.attributes.intellect) -
                                  b.value.attributes.intellect) *
                                     2 +
                                 a.value.tactics,
                             40, 360);
  return {true,
          {side, Integer(chance), *own.strategist_actor_id, target_actor.value,
           due.value},
          {}};
}
} // namespace
Status validate_field_confuse_command(
    const CommonStore &c, const MilitaryState &m,
    const MilitaryCatalog &commands, const TravelCatalog &t,
    const FieldBattleCatalog &f, const FieldBattleWorldState &w,
    const StableId &a, const FieldBattleStratagem &q,
    const FieldBattleAbilityReadContext &sources) {
  auto ready = prepare_field_confuse(c, m, commands, t, f, w, a, q, sources);
  return ready.ok ? common_success() : Status{false, {}, ready.error};
}
Status apply_field_confuse_command_in_candidate(
    CommonStore &c, const MilitaryState &m, const MilitaryCatalog &commands,
    const TravelCatalog &t, const FieldBattleCatalog &f,
    FieldBattleWorldState &w, const StableId &a, const FieldBattleStratagem &q,
    const FieldBattleAbilityReadContext &sources, CheckpointMemoryCharge &owner,
    FieldBattleMutationResult &output) {
  auto ready = prepare_field_confuse(c, m, commands, t, f, w, a, q, sources);
  if (!ready.ok)
    return {false, {}, ready.error};
  auto &session = w.sessions.at(q.battle_id);
  if (session.next_stratagem_sequence <= 0 ||
      session.next_stratagem_sequence == std::numeric_limits<Integer>::max())
    return common_failure("FieldStratagemSequenceExhausted", q.battle_id);

  char decimal[32];
  auto converted = std::to_chars(decimal, decimal + sizeof(decimal),
                                 c.state().next_instance_id);
  if (converted.ec != std::errc{})
    return common_failure("FieldStratagemSequenceExhausted", q.battle_id);
  const ProducedStableIdBuildView id_shape{
      "field_stratagem",
      std::string_view(decimal, std::size_t(converted.ptr - decimal))};


  (owner).grow(0);
  auto id = c.next_id("field_stratagem");
  if (!id.ok)
    return {false, {}, id.error};
  FieldStratagemRecord row;
  row.stratagem_id = id.value;
  row.kind = FieldStratagemKind::Confuse;
  row.issuer_actor_id = a;
  row.side = ready.value.side;
  row.strategist_actor_id = ready.value.strategist;
  row.target_formation_id = q.target_formation_id;
  row.target_commander_actor_id = ready.value.target_commander;
  row.issued = session.now;
  row.resolve_at = ready.value.due;
  row.issued_sequence = session.next_stratagem_sequence++;
  row.observed_tick = c.tick();
  auto actual_moment = read_field_actual_application_moment(c, sources);
  if (!actual_moment.ok)
    return {false, {}, actual_moment.error};
  row.applied_at = actual_moment.value;
  row.adopted_source = {"military", "field_battle_stratagem", id.value};
  row.success_chance_quarters = ready.value.chance_quarters;
  auto head = c.append_fact({id.value, "field_battle_stratagem", "military"});
  if (!head.ok)
    return head;
  if (!session.stratagems.emplace(id.value, std::move(row)).second)
    return common_failure("FieldStratagemRepeated", id.value);
  --session.sides[ready.value.side].stratagem_points;
  FieldBattleMutationResult result;
  result.battle_id = q.battle_id;
  result.state = session.state;
  result.at = session.now;
  result.stratagem_id = id.value;
  result.effective_at = ready.value.due;
  output = std::move(result);
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
struct PreparedCouncilStratagem {
  Integer side = 0;
  std::size_t proposal = 0;
  FieldCouncilStratagemPlan item;
};
Outcome<PreparedCouncilStratagem> prepare_council_stratagem(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &commands, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &actor, const FieldBattleStratagem &command,
    const FieldBattleAbilityReadContext &sources) {
  auto fail = [&](const char *code) -> Outcome<PreparedCouncilStratagem> {
    return {false, {}, common_failure(code, command.battle_id).error};
  };
  if (command.kind != FieldStratagemKind::Ambush &&
      command.kind != FieldStratagemKind::FeintStrength)
    return fail("InvalidFieldStratagem");
  if (command.target_formation_id ||
      (command.proposal_id && command.proposal_id->empty()))
    return fail("InvalidFieldStratagem");
  auto found = world.sessions.find(command.battle_id);
  if (found == world.sessions.end())
    return fail("FieldBattleNotFound");
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, &sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &session = found->second;
  if (session.state != FieldBattleState::Council)
    return fail("FieldCouncilClosed");
  auto permission = control(common, military, commands, session, world, actor);
  if (!permission.ok)
    return {false, {}, permission.error};
  const auto side = permission.value.side;
  if (side < 0 || side > 1 || permission.value.scope != "army" ||
      actor != common.state().metadata.player_actor_id ||
      session.sides[side].commander_actor_id != actor ||
      session.councils[side].player_actor_id != actor ||
      session.councils[side].closed)
    return fail("FieldOrderNoAuthority");
  const auto &own = session.sides[side];
  if (!own.strategist_actor_id)
    return fail("FieldStratagemNoStrategist");
  const auto *strategist = common.actor(*own.strategist_actor_id);
  if (!strategist || !strategist->alive || !strategist->available)
    return fail("FieldStratagemNoStrategist");
  bool present = false;
  for (const auto &participant : session.councils[side].participants)
    if (participant.role == FieldCouncilRole::Strategist &&
        participant.actor_id == *own.strategist_actor_id)
      present = true;
  if (!present)
    return fail("FieldStratagemNoStrategist");
  const auto &proposals = session.councils[side].proposals;
  std::optional<std::size_t> selected;
  for (std::size_t i = 0; i < proposals.size(); ++i)
    if (command.proposal_id ? proposals[i].proposal_id == *command.proposal_id
                            : proposals[i].speaker_actor_id == actor)
      selected = i; // Actual stored proposal order, not lexical ID order.
  if (!selected)
    return fail("FieldCouncilProposalRequired");
  PreparedCouncilStratagem out;
  out.side = side;
  out.proposal = *selected;
  out.item.kind = command.kind == FieldStratagemKind::Ambush
                      ? FieldCouncilStratagemKind::Ambush
                      : FieldCouncilStratagemKind::FeintStrength;
  out.item.formation_id = command.formation_id;
  out.item.area_dm = command.area_dm;
  if (command.kind == FieldStratagemKind::Ambush) {
    if (!command.formation_id || !command.area_dm)
      return fail("InvalidFieldStratagem");
    const auto formation = session.formations.find(*command.formation_id);
    if (formation == session.formations.end() || formation->second.side != side)
      return fail("FieldStratagemFormationRequired");
    auto field = generated(travel, catalog, world, command.battle_id, sources);
    if (!field.ok)
      return {false, {}, field.error};
    auto mission = validate_field_battle_mission(
        FieldHold{*command.area_dm, formation->second.facing_mdeg},
        formation->second, session, field.value);
    if (!mission.ok)
      return {false, {}, mission.error};
  } else if (command.formation_id || command.area_dm)
    return fail("InvalidFieldStratagem");
  for (const auto &item : proposals[*selected].plan.stratagems)
    if (item.kind == out.item.kind &&
        (item.kind == FieldCouncilStratagemKind::FeintStrength ||
         item.formation_id == out.item.formation_id))
      return fail("FieldStratagemRepeated");
  return {true, std::move(out), {}};
}
} // namespace
Status validate_field_council_stratagem_command(
    const CommonStore &c, const MilitaryState &m, const MilitaryCatalog &mc,
    const TravelCatalog &t, const FieldBattleCatalog &f,
    const FieldBattleWorldState &w, const StableId &actor,
    const FieldBattleStratagem &q,
    const FieldBattleAbilityReadContext &sources) {
  auto ready = prepare_council_stratagem(c, m, mc, t, f, w, actor, q, sources);
  return ready.ok ? common_success() : Status{false, {}, ready.error};
}
Status attach_field_council_stratagem_in_candidate(
    const CommonStore &c, const MilitaryState &m, const MilitaryCatalog &mc,
    const TravelCatalog &t, const FieldBattleCatalog &f,
    FieldBattleWorldState &w, const StableId &actor,
    const FieldBattleStratagem &q, const FieldBattleAbilityReadContext &sources,
    CheckpointMemoryCharge &owner, FieldBattleMutationResult &result) {
  auto ready = prepare_council_stratagem(c, m, mc, t, f, w, actor, q, sources);
  if (!ready.ok)
    return {false, {}, ready.error};
  auto &session = w.sessions.at(q.battle_id);
  auto &proposal =
      session.councils[ready.value.side].proposals[ready.value.proposal];
  if (proposal.plan.stratagems.size() >=
      std::size_t(std::numeric_limits<Integer>::max()))
    return common_failure("FieldStratagemSequenceExhausted", q.battle_id);


  (owner).grow(0);
  const auto index = Integer(proposal.plan.stratagems.size());
  const auto accepted_item = ready.value.item;
  proposal.plan.stratagems.push_back(std::move(ready.value.item));
  FieldBattleMutationResult applied;
  applied.battle_id = q.battle_id;
  applied.at = session.now;
  applied.state = FieldBattleState::Council;
  applied.proposal_id = proposal.proposal_id;
  applied.request_index = index;
  applied.council_stratagem = accepted_item;
  result = std::move(applied);
  // Common result binding follows in this same atomic candidate. No RNG,
  // points, effect record, world time or character body is changed here.
  return common_success();
}
} // namespace hanmo::resident
