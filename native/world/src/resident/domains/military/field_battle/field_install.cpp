#include "field_event_legacy_kinds.hpp"
// 准备所有真实部队、地形与绑定后才写同候选；费用沿原 actions_step，不重复扣取。
#include "field_install.hpp"
#include "field_council_prepare.hpp"
#include "field_deployment.hpp"
#include "field_exact_session_build.hpp"
#include "field_exit_policy.hpp"
#include "field_segment_settlement.hpp"
#include "field_stratagem_resolution.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/kernel/native_military_state.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 无适配过的实际来源就失败，不给未知军团或名字造一个空绑定。
Outcome<FieldBattleRoadBinding>
binding_for(const CommonRead &common, const MilitaryState &military,
            const FieldBattleSession &session, const FieldRoadContact &source,
            const StableId &contact_id, Integer endpoint) {
  const auto &army_id = source.army_ids[endpoint];
  const ArmyRecord *army = nullptr;
  for (const auto &row : military.armies)
    if (row.id == army_id) {
      if (army)
        return {false,
                {},
                common_failure("FieldArmySourceMismatch", army_id).error};
      army = &row;
    }
  if (!army || !source.contact.road)
    return {
        false, {}, common_failure("FieldArmySourceMismatch", army_id).error};
  const auto &end =
      endpoint == 0 ? source.contact.road->first : source.contact.road->second;
  FieldBattleRoadBinding out;
  out.army_id = army_id;
  out.battle_id = session.battle_id;
  out.action_id = end.trace.action_id;
  out.commander_actor_id = army->commander_actor_id;
  out.source = FieldInitialContactBinding{contact_id, endpoint};
  std::set<StableId> members(army->member_actor_ids.begin(),
                             army->member_actor_ids.end());
  members.insert(army->commander_actor_id);
  if (members.contains(""))
    return {
        false, {}, common_failure("FieldArmySourceMismatch", army_id).error};
  for (const auto &actor_id : members) {
    if (!common.actor(actor_id))
      return {false, {}, common_failure("UnknownActor", actor_id).error};
    const FieldFormation *found = nullptr;
    for (const auto &[id, formation] : session.formations) {
      if (formation.army_id != army_id)
        continue;
      bool belongs = formation.commander_actor_id == actor_id;
      for (const auto *group :
           {&formation.fit, &formation.wounded, &formation.dead,
            &formation.captured, &formation.deserted})
        belongs = belongs || std::find(group->begin(), group->end(),
                                       actor_id) != group->end();
      if (!belongs)
        continue;
      if (found)
        return {false,
                {},
                common_failure("FieldMemberFormationRepeated", actor_id).error};
      found = &formation;
    }
    if (!found)
      return {false,
              {},
              common_failure("FieldMemberFormationRequired", actor_id).error};
    out.members.push_back({actor_id, found->formation_id});
  }
  return {true, std::move(out), {}};
}
// 军议先读原人物/任职/关系，NPC 当场定案；玩家统帅的真实记录留待公开命令。
Status prepare_initial_council(const CommonStore &common,
                               const MilitaryState &military,
                               const SocialState &social,
                               const OrganizationState &organization,
                               const FieldBattleCatalog &catalog,
                               const StableId &player,
                               FieldSessionPreparation &prepared,
                               const FieldBattleAbilityReadContext &abilities) {
  auto councils = prepare_field_councils(
      common, social, military, &organization, catalog.office_titles,
      prepared.session, prepared.field, player, abilities);
  if (!councils.ok)
    return {false, {}, councils.error};
  prepared.session.councils = std::move(councils.value);
  auto source = read_field_council_sources(
      common, prepared.session, prepared.field, social, military,
      catalog.office_titles, &organization, abilities);
  if (!source.ok)
    return {false, {}, source.error};
  for (Integer side = 0; side < 2; ++side) {
    auto &target = prepared.session.sides[side];
    target.strategist_actor_id = source.value.sides[side].strategist_actor_id;
    if (target.strategist_actor_id) {
      const auto &officers = source.value.sides[side].officers;
      const auto strategist = std::find_if(
          officers.begin(), officers.end(), [&](const auto &officer) {
            return officer.actor_id == *target.strategist_actor_id;
          });
      if (strategist == officers.end())
        return common_failure("FieldStratagemNoStrategist",
                              *target.strategist_actor_id);
      target.initial_stratagem_points =
          std::max<Integer>(1, strategist->intellect / 30);
      target.stratagem_points = target.initial_stratagem_points;
    }
    if (std::holds_alternative<FieldPlayerController>(
            prepared.session.sides[side].controller))
      continue;
    // 玩家部将的本侧等其一次自请或直接听令；对方无人侧仍立即定案。
    if (prepared.session.councils[side].player_actor_id)
      continue;
    auto selected = select_field_council_proposal(source.value, prepared.field,
                                                  prepared.session, side);
    if (!selected.ok)
      return {false, {}, selected.error};
    auto adopted = prepare_field_council_adoption(
        prepared.session, prepared.field, side, selected.value, {});
    if (!adopted.ok)
      return adopted;
  }
  return common_success();
}
} // namespace
static Outcome<StableId> publish_field_session_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    FieldContactOrigin source, StableId contact_id, StableId battle_id,
    FieldSessionPreparation prepared,
    std::array<FieldBattleRoadBinding, 2> bindings,
    const FieldBattleAbilityReadContext &abilities,
    CheckpointMemoryCharge &owner) {
  auto fact =
      common.append_fact({contact_id, "field_battle_contact", "military"});
  if (!fact.ok)
    return {false, {}, fact.error};
  fact = common.append_fact({battle_id, "field_battle_session", "military"});
  if (!fact.ok)
    return {false, {}, fact.error};

  for (const auto &binding : bindings) {

    std::visit(
        [&](const auto &value) {

        },
        binding.source);


  }
  (owner).grow(0);
  if (!world.contacts
           .emplace(contact_id, FieldBattleContactRecord{contact_id, battle_id,
                                                         source, bindings})
           .second ||
      !world.sessions.emplace(battle_id, std::move(prepared.session)).second)
    return {
        false,
        {},
        common_failure("FieldBattleIdRepeated", battle_id, ErrorKind::internal)
            .error};
  // NPC 定案事实只引用此真实 Session 的军议记录；不改变任何 S4 关系值。
  for (Integer side = 0; side < 2; ++side) {
    auto &session = world.sessions.at(battle_id);
    if (!session.councils[side].closed)
      continue;
    auto id = common.next_id("field_council");
    if (!id.ok)
      return {false, {}, id.error};
    FieldCouncilFact record;
    record.fact_id = id.value;
    record.battle_id = battle_id;
    record.side = side;
    record.at = session.now;
    record.recorded_tick = session.created_tick;
    record.kind = FieldCouncilFactKind::Adopted;
    auto adoption_moment =
        read_field_actual_application_moment(common, abilities);
    if (!adoption_moment.ok)
      return {false, {}, adoption_moment.error};
    record.adopted_actual_at = adoption_moment.value;
    record.council = session.councils[side];
    auto appended =
        common.append_fact({id.value, "military_council", "military"});
    if (!appended.ok)
      return {false, {}, appended.error};
    if (!world.council_facts.emplace(id.value, std::move(record)).second)
      return {
          false,
          {},
          common_failure("FieldBattleIdRepeated", id.value, ErrorKind::internal)
              .error};
    FieldBattleEvent event;
    event.kind = field_event_kind::CouncilAdopted;
    event.side = side;
    event.source_id = id.value;
    auto emitted = append_field_battle_event(session, std::move(event));
    if (!emitted.ok)
      return {false, {}, emitted.error};
  }
  auto &installed_session = world.sessions.at(battle_id);
  // Real adoption heads must precede the initial participation cut. Session
  // existence alone is not the later start/skill/morale freeze boundary.
  if (installed_session.state == FieldBattleState::Running) {
    auto start = common.next_id("field_start");
    if (!start.ok)
      return {false, {}, start.error};
    auto head =
        common.append_fact({start.value, "field_battle_started", "military"});
    if (!head.ok)
      return {false, {}, head.error};
    auto initialized = initialize_field_battle_abilities(
        common, abilities, installed_session,
        {"military", "field_battle_started", start.value},
        installed_session.created_tick, {}, owner);
    if (!initialized.ok)
      return {false, {}, initialized.error};
  }
  for (const auto &[id, adopted] : world.council_facts)
    if (adopted.battle_id == battle_id &&
        adopted.kind == FieldCouncilFactKind::Adopted) {
      auto installed = install_field_adopted_stratagems_in_candidate(
          common, world, id, prepared.field, abilities, owner);
      if (!installed.ok)
        return {false, {}, installed.error};
    }
  for (auto &binding : bindings) {
    const auto id = binding.army_id;
    if (!world.army_bindings.emplace(id, std::move(binding)).second)
      return {false,
              {},
              common_failure("FieldArmyAlreadyBound", id, ErrorKind::internal)
                  .error};
  }
  return {true, std::move(battle_id), {}};
}
Outcome<StableId> install_field_road_session(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, TravelState &travel_state,
    const TravelCatalog &travel, const FieldBattleCatalog &field_catalog,
    FieldBattleWorldState &world, const FieldRoadContact &source,
    const StableId &player, Integer battle_seed, const SocialState &social,
    const OrganizationState &organization,
    const FieldBattleAbilityReadContext &abilities,
    CheckpointMemoryCharge &owner, const MilitaryFormationSourceRead *exact) {
  // 已选精确成军来源不能因 caller 漏 reader 就回落旧 action_facts。
  auto military_selection =
      selected_native_military_state_versions(field_catalog.identity);
  if (!military_selection.ok)
    return {false, {}, military_selection.error};
  if (exact && abilities.original_formations &&
      exact != abilities.original_formations)
    return {false, {}, common_failure("FieldFormationReaderMismatch").error};
  if (!exact)
    exact = abilities.original_formations;
  if ((military_selection.value.military_action_time ||
       military_selection.value.dynamic_military) &&
      (!exact || !abilities.actual_clock))
    return {false,
            {},
            common_failure("FieldBattleExactFormationReaderRequired",
                           "field_battle/formation",
                           ErrorKind::unsupported_capability)
                .error};
  if (world.schema != 1 || field_catalog.parameters.version != "field_battle/1")
    return {false, {}, common_failure("FieldBattleProfileMismatch").error};
  for (const auto &army : source.army_ids)
    if (world.army_bindings.contains(army))
      return {false, {}, common_failure("FieldArmyAlreadyBound", army).error};
  // 窗末接触先按原 source_tick+fraction 归一化，安装时间不是物理窗口起点。
  auto time = field_contact_battle_time(source.source_tick, source.contact,
                                        field_catalog.parameters);
  if (!time.ok)
    return {false, {}, time.error};
  std::optional<WorldMoment> actual_now;
  if (abilities.actual_clock) {
    auto current = abilities.actual_clock->read_current_moment();
    if (!current.ok)
      return {false, {}, current.error};
    actual_now = std::move(current.value);
  }
  auto contact_id = common.next_id("field_contact");
  if (!contact_id.ok)
    return {false, {}, contact_id.error};
  auto battle_id = common.next_id("field_battle");
  if (!battle_id.ok)
    return {false, {}, battle_id.error};
  auto prepared = prepare_field_road_session(
      common, military, catalog, travel_state, travel, field_catalog, source,
      battle_id.value, contact_id.value, player, battle_seed, exact,
      actual_now ? &*actual_now : nullptr);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  auto deployment =
      prepare_initial_council(common, military, social, organization,
                              field_catalog, player, prepared.value, abilities);
  if (!deployment.ok)
    return {false, {}, deployment.error};
  std::array<FieldBattleRoadBinding, 2> bindings;
  for (Integer endpoint = 0; endpoint < 2; ++endpoint) {
    auto binding = binding_for(common, military, prepared.value.session, source,
                               contact_id.value, endpoint);
    if (!binding.ok)
      return {false, {}, binding.error};
    bindings[endpoint] = std::move(binding.value);
  }
  // 非互动路触只截真实锚点并保存 met_actions；禁止把会战 ID 塞入旧 Encounter
  // 账。
  auto truncated = apply_road_contact_truncation(common, travel_state,
                                                 source.contact, {}, false);
  if (!truncated.ok)
    return {false, {}, truncated.error};
  return publish_field_session_in_candidate(
      common, world, FieldContactOrigin{source}, std::move(contact_id.value),
      std::move(battle_id.value), std::move(prepared.value),
      std::move(bindings), abilities, owner);
}
Outcome<StableId> install_hostile_field_exact_group_session_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &actual_contact,
    const StableId &player, Integer battle_seed, const SocialState &social,
    const OrganizationState &organization,
    const FieldBattleAbilityReadContext &abilities,
    CheckpointMemoryCharge &owner) {
  auto fail = [&](const char *code) -> Outcome<StableId> {
    return {false, {}, common_failure(code, actual_contact.id).error};
  };
  if (&x.common != &common || &x.field != &world ||
      &relations.common != &common || &relations.military != &x.military ||
      &relations.field != &world || &relations.formations != &x.formations ||
      &relations.actual_clock != &x.clock ||
      abilities.diplomacy != &relations || abilities.exact_contacts != &x ||
      abilities.original_formations != &x.formations ||
      abilities.actual_clock != &x.clock ||
      !x.clock.bound_to(common, x.military, world))
    return fail("CrossCandidateFieldExactContact");
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value.military_action_time || world.schema != 1 ||
      catalog.parameters.version != "field_battle/1")
    return fail("FieldExactContactProfileMismatch");
  auto actual = read_field_exact_road_contact_source(x, actual_contact,
                                                     catalog.parameters);
  if (!actual.ok)
    return {false, {}, actual.error};
  for (const auto &[id, contact] : world.contacts)
    if (const auto exact =
            std::get_if<FieldExactGroupContactOrigin>(&contact.source);
        exact && same_source_ref(exact->actual_contact, actual_contact))
      return fail("FieldExactContactAlreadyInstalled");
  for (const auto &end : actual.value.endpoints)
    if (world.army_bindings.contains(end.army->id))
      return fail("FieldArmyAlreadyBound");
  auto relation = read_current_political_relation_view(
      relations, *actual.value.endpoints[0].faction_id,
      *actual.value.endpoints[1].faction_id);
  if (!relation.ok)
    return {false, {}, relation.error};
  if (relation.value.relation != PoliticalRelation::hostile)
    return fail("FieldContactNotHostile");
  auto contact_id = common.next_id("field_contact"),
       battle_id = common.next_id("field_battle");
  if (!contact_id.ok)
    return {false, {}, contact_id.error};
  if (!battle_id.ok)
    return {false, {}, battle_id.error};
  auto prepared = prepare_field_exact_road_session(
      x, catalog, actual_contact, battle_id.value, contact_id.value, player,
      battle_seed);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  for (Integer side = 0; side != 2; ++side) {
    auto exits = read_field_exits_before_contact_install(
        x, relations, abilities.territory, catalog, actual_contact, side);
    if (!exits.ok)
      return {false, {}, exits.error};
    prepared.value.session.sides[side].exits = std::move(exits.value.cells);
  }
  auto councils =
      prepare_initial_council(common, x.military, social, organization, catalog,
                              player, prepared.value, abilities);
  if (!councils.ok)
    return {false, {}, councils.error};
  std::array<FieldBattleRoadBinding, 2> bindings;
  for (const auto &endpoint : actual.value.endpoints) {
    const Integer i = endpoint.geometry_endpoint ==
                              &actual.value.contact->original_hit.road->first
                          ? 0
                          : 1;
    auto &binding = bindings[i];
    binding.army_id = endpoint.army->id;
    binding.battle_id = battle_id.value;
    binding.commander_actor_id = endpoint.army->commander_actor_id;
    binding.source = FieldInitialContactBinding{contact_id.value, i};
    // A group journey is not a commander legacy action. Keep action_id empty.
    std::set<StableId> seen;
    for (const auto &actor : endpoint.opening->named_participants) {
      if (!common.actor(actor) || !seen.insert(actor).second)
        return fail("FieldOriginalGroupMemberMismatch");
      const FieldFormation *found = nullptr;
      for (const auto &[id, formation] : prepared.value.session.formations) {
        if (formation.army_id != binding.army_id)
          continue;
        bool member = formation.commander_actor_id == actor;
        for (const auto *group :
             {&formation.fit, &formation.wounded, &formation.dead,
              &formation.captured, &formation.deserted})
          member = member || std::find(group->begin(), group->end(), actor) !=
                                 group->end();
        if (!member)
          continue;
        if (found)
          return fail("FieldMemberFormationRepeated");
        found = &formation;
      }
      if (!found)
        return fail("FieldMemberFormationRequired");
      binding.members.push_back({actor, found->formation_id});
    }
    if (binding.members.empty())
      return fail("FieldOriginalGroupMemberMismatch");
  }
  // The original S1 contact already closed its units. Do not run legacy
  // truncation, charge again, or write a second position. The outer S1 business
  // installation and P3 start-adoption must complete in this same candidate.
  return publish_field_session_in_candidate(
      common, world, FieldExactGroupContactOrigin{actual_contact},
      std::move(contact_id.value), std::move(battle_id.value),
      std::move(prepared.value), std::move(bindings), abilities, owner);
}
} // namespace hanmo::resident
